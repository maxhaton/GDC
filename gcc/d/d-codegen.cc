// d-codegen.cc -- D frontend for GCC.
// Copyright (C) 2011-2015 Free Software Foundation, Inc.

// GCC is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3, or (at your option) any later
// version.

// GCC is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for more details.

// You should have received a copy of the GNU General Public License
// along with GCC; see the file COPYING3.  If not see
// <http://www.gnu.org/licenses/>.

#include "config.h"
#include "system.h"
#include "coretypes.h"

#include "dfrontend/aggregate.h"
#include "dfrontend/attrib.h"
#include "dfrontend/ctfe.h"
#include "dfrontend/declaration.h"
#include "dfrontend/init.h"
#include "dfrontend/module.h"
#include "dfrontend/statement.h"
#include "dfrontend/target.h"
#include "dfrontend/template.h"

#include "tree.h"
#include "tree-iterator.h"
#include "fold-const.h"
#include "diagnostic.h"
#include "langhooks.h"
#include "target.h"
#include "stringpool.h"
#include "varasm.h"
#include "stor-layout.h"
#include "attribs.h"
#include "function.h"

#include "d-tree.h"
#include "d-objfile.h"
#include "d-codegen.h"
#include "d-dmd-gcc.h"
#include "id.h"


// Return the DECL_CONTEXT for symbol DSYM.

tree
d_decl_context (Dsymbol *dsym)
{
  Dsymbol *parent = dsym;
  Declaration *decl = dsym->isDeclaration();

  while ((parent = parent->toParent2()))
    {
      // We've reached the top-level module namespace.
      // Set DECL_CONTEXT as the NAMESPACE_DECL of the enclosing module,
      // but only for extern(D) symbols.
      if (parent->isModule())
	{
	  if (decl != NULL && decl->linkage != LINKd)
	    return NULL_TREE;

	  return build_import_decl(parent);
	}

      // Declarations marked as 'static' or '__gshared' are never
      // part of any context except at module level.
      if (decl != NULL && decl->isDataseg())
	continue;

      // Nested functions.
      FuncDeclaration *fd = parent->isFuncDeclaration();
      if (fd != NULL)
	return get_symbol_decl (fd);

      // Methods of classes or structs.
      AggregateDeclaration *ad = parent->isAggregateDeclaration();
      if (ad != NULL)
	{
	  tree context = build_ctype(ad->type);
	  // Want the underlying RECORD_TYPE.
	  if (ad->isClassDeclaration())
	    context = TREE_TYPE (context);

	  return context;
	}
    }

  return NULL_TREE;
}

/* Return a copy of record TYPE but safe to modify in any way.  */

tree
copy_aggregate_type (tree type)
{
  tree newtype = build_distinct_type_copy (type);
  TYPE_FIELDS (newtype) = copy_list (TYPE_FIELDS (type));
  TYPE_METHODS (newtype) = copy_list (TYPE_METHODS (type));

  for (tree f = TYPE_FIELDS (newtype); f; f = DECL_CHAIN (f))
    DECL_FIELD_CONTEXT (f) = newtype;

  for (tree m = TYPE_METHODS (newtype); m; m = DECL_CHAIN (m))
    DECL_CONTEXT (m) = newtype;

  return newtype;
}

// Return TRUE if declaration DECL is a reference type.

bool
declaration_reference_p(Declaration *decl)
{
  Type *tb = decl->type->toBasetype();

  // Declaration is a reference type.
  if (tb->ty == Treference || decl->storage_class & (STCout | STCref))
    return true;

  return false;
}

// Returns the real type for declaration DECL.
// Reference decls are converted to reference-to-types.
// Lazy decls are converted into delegates.

tree
declaration_type(Declaration *decl)
{
  // Lazy declarations are converted to delegates.
  if (decl->storage_class & STClazy)
    {
      TypeFunction *tf = TypeFunction::create (NULL, decl->type, false, LINKd);
      TypeDelegate *t = new TypeDelegate(tf);
      return build_ctype(t->merge());
    }

  // Static array va_list have array->pointer conversions applied.
  if (decl->isParameter() && valist_array_p (decl->type))
    {
      Type *valist = decl->type->nextOf()->pointerTo();
      valist = valist->castMod(decl->type->mod);
      return build_ctype(valist);
    }

  tree type = build_ctype(decl->type);

  // Parameter is passed by reference.
  if (declaration_reference_p(decl))
    return build_reference_type(type);

  // The 'this' parameter is always const.
  if (decl->isThisDeclaration())
    return insert_type_modifiers(type, MODconst);

  return type;
}

// These should match the Declaration versions above
// Return TRUE if parameter ARG is a reference type.

bool
argument_reference_p(Parameter *arg)
{
  Type *tb = arg->type->toBasetype();

  // Parameter is a reference type.
  if (tb->ty == Treference || arg->storageClass & (STCout | STCref))
    return true;

  tree type = build_ctype(arg->type);
  if (TREE_ADDRESSABLE (type))
    return true;

  return false;
}

// Returns the real type for parameter ARG.
// Reference parameters are converted to reference-to-types.
// Lazy parameters are converted into delegates.

tree
type_passed_as(Parameter *arg)
{
  // Lazy parameters are converted to delegates.
  if (arg->storageClass & STClazy)
    {
      TypeFunction *tf = TypeFunction::create (NULL, arg->type, false, LINKd);
      TypeDelegate *t = new TypeDelegate(tf);
      return build_ctype(t->merge());
    }

  // Static array va_list have array->pointer conversions applied.
  if (valist_array_p (arg->type))
    {
      Type *valist = arg->type->nextOf()->pointerTo();
      valist = valist->castMod(arg->type->mod);
      return build_ctype(valist);
    }

  tree type = build_ctype(arg->type);

  // Parameter is passed by reference.
  if (argument_reference_p(arg))
    return build_reference_type(type);

  return type;
}

// Build INTEGER_CST of type TYPE with the value VALUE.

tree
build_integer_cst (dinteger_t value, tree type)
{
  // The type is error_mark_node, we can't do anything.
  if (error_operand_p (type))
    return type;

  return build_int_cst_type (type, value);
}

// Build REAL_CST of type TOTYPE with the value VALUE.

tree
build_float_cst (const real_t& value, Type *totype)
{
  real_t new_value;
  TypeBasic *tb = totype->isTypeBasic();

  gcc_assert (tb != NULL);

  tree type_node = build_ctype(tb);
  real_convert (&new_value.rv(), TYPE_MODE (type_node), &value.rv());

  // Value grew as a result of the conversion. %% precision bug ??
  // For now just revert back to original.
  if (new_value > value)
    new_value = value;

  return build_real (type_node, new_value.rv());
}

// Returns the .length component from the D dynamic array EXP.

tree
d_array_length (tree exp)
{
  if (error_operand_p (exp))
    return exp;

  // Get the backend type for the array and pick out the array
  // length field (assumed to be the first field.)
  tree len_field = TYPE_FIELDS (TREE_TYPE (exp));
  return component_ref (exp, len_field);
}

// Returns the .ptr component from the D dynamic array EXP.

tree
d_array_ptr (tree exp)
{
  if (error_operand_p (exp))
    return exp;

  // Get the backend type for the array and pick out the array
  // data pointer field (assumed to be the second field.)
  tree ptr_field = TREE_CHAIN (TYPE_FIELDS (TREE_TYPE (exp)));
  return component_ref (exp, ptr_field);
}

// Returns a constructor for D dynamic array type TYPE of .length LEN
// and .ptr pointing to DATA.

tree
d_array_value (tree type, tree len, tree data)
{
  // %% assert type is a darray
  tree len_field, ptr_field;
  vec<constructor_elt, va_gc> *ce = NULL;

  len_field = TYPE_FIELDS (type);
  ptr_field = TREE_CHAIN (len_field);

  len = convert (TREE_TYPE (len_field), len);
  data = convert (TREE_TYPE (ptr_field), data);

  CONSTRUCTOR_APPEND_ELT (ce, len_field, len);
  CONSTRUCTOR_APPEND_ELT (ce, ptr_field, data);

  return build_constructor (type, ce);
}

// Builds a D string value from the C string STR.

tree
d_array_string (const char *str)
{
  unsigned len = strlen (str);
  // Assumes STR is 0-terminated.
  tree str_tree = build_string (len + 1, str);

  TREE_TYPE (str_tree) = make_array_type (Type::tchar, len);

  return d_array_value (build_ctype(Type::tchar->arrayOf()),
			size_int (len), build_address (str_tree));
}

// Returns value representing the array length of expression EXP.
// TYPE could be a dynamic or static array.

tree
get_array_length (tree exp, Type *type)
{
  Type *tb = type->toBasetype();

  switch (tb->ty)
    {
    case Tsarray:
      return size_int (((TypeSArray *) tb)->dim->toUInteger());

    case Tarray:
      return d_array_length (exp);

    default:
      error ("can't determine the length of a %s", type->toChars());
      return error_mark_node;
    }
}

// Create BINFO for a ClassDeclaration's inheritance tree.
// Interfaces are not included.

tree
build_class_binfo (tree super, ClassDeclaration *cd)
{
  tree binfo = make_tree_binfo (1);
  tree ctype = build_ctype(cd->type);

  // Want RECORD_TYPE, not POINTER_TYPE
  BINFO_TYPE (binfo) = TREE_TYPE (ctype);
  BINFO_INHERITANCE_CHAIN (binfo) = super;
  BINFO_OFFSET (binfo) = integer_zero_node;

  if (cd->baseClass)
    BINFO_BASE_APPEND (binfo, build_class_binfo (binfo, cd->baseClass));

  return binfo;
}

// Create BINFO for an InterfaceDeclaration's inheritance tree.
// In order to access all inherited methods in the debugger,
// the entire tree must be described.
// This function makes assumptions about interface layout.

tree
build_interface_binfo (tree super, ClassDeclaration *cd, unsigned& offset)
{
  tree binfo = make_tree_binfo (cd->baseclasses->dim);
  tree ctype = build_ctype(cd->type);

  // Want RECORD_TYPE, not POINTER_TYPE
  BINFO_TYPE (binfo) = TREE_TYPE (ctype);
  BINFO_INHERITANCE_CHAIN (binfo) = super;
  BINFO_OFFSET (binfo) = size_int (offset * Target::ptrsize);
  BINFO_VIRTUAL_P (binfo) = 1;

  for (size_t i = 0; i < cd->baseclasses->dim; i++, offset++)
    {
      BaseClass *bc = (*cd->baseclasses)[i];
      BINFO_BASE_APPEND (binfo, build_interface_binfo (binfo, bc->sym, offset));
    }

  return binfo;
}

// Returns the .funcptr component from the D delegate EXP.

tree
delegate_method (tree exp)
{
  // Get the backend type for the array and pick out the array length
  // field (assumed to be the second field.)
  tree method_field = TREE_CHAIN (TYPE_FIELDS (TREE_TYPE (exp)));
  return component_ref (exp, method_field);
}

// Returns the .object component from the delegate EXP.

tree
delegate_object (tree exp)
{
  // Get the backend type for the array and pick out the array data
  // pointer field (assumed to be the first field.)
  tree obj_field = TYPE_FIELDS (TREE_TYPE (exp));
  return component_ref (exp, obj_field);
}

// Build a delegate literal of type TYPE whose pointer function is
// METHOD, and hidden object is OBJECT.

tree
build_delegate_cst(tree method, tree object, Type *type)
{
  tree ctor = make_node(CONSTRUCTOR);
  tree ctype;

  Type *tb = type->toBasetype();
  if (tb->ty == Tdelegate)
    ctype = build_ctype(type);
  else
    {
      // Convert a function literal into an anonymous delegate.
      ctype = make_two_field_type (TREE_TYPE (object), TREE_TYPE (method),
				   NULL, "object", "func");
    }

  vec<constructor_elt, va_gc> *ce = NULL;
  CONSTRUCTOR_APPEND_ELT (ce, TYPE_FIELDS (ctype), object);
  CONSTRUCTOR_APPEND_ELT (ce, TREE_CHAIN (TYPE_FIELDS (ctype)), method);

  CONSTRUCTOR_ELTS (ctor) = ce;
  TREE_TYPE (ctor) = ctype;

  return ctor;
}

// Builds a temporary tree to store the CALLEE and OBJECT
// of a method call expression of type TYPE.

tree
build_method_call (tree callee, tree object, Type *type)
{
  tree t = build_delegate_cst (callee, object, type);
  METHOD_CALL_EXPR (t) = 1;
  return t;
}

// Extract callee and object from T and return in to CALLEE and OBJECT.

void
extract_from_method_call (tree t, tree& callee, tree& object)
{
  gcc_assert (METHOD_CALL_EXPR (t));
  object = CONSTRUCTOR_ELT (t, 0)->value;
  callee = CONSTRUCTOR_ELT (t, 1)->value;
}

// Build a dereference into the virtual table for OBJECT to retrieve
// a function pointer of type FNTYPE at position INDEX.

tree
build_vindex_ref(tree object, tree fntype, size_t index)
{
  // The vtable is the first field.  Interface methods are also in the class's
  // vtable, so we don't need to convert from a class to an interface.
  tree result = build_deref(object);
  result = component_ref(result, TYPE_FIELDS (TREE_TYPE (result)));

  gcc_assert(POINTER_TYPE_P (fntype));

  return build_memref(fntype, result, size_int(Target::ptrsize * index));
}

// Return TRUE if EXP is a valid lvalue.  Lvalues references cannot be
// made into temporaries, otherwise any assignments will be lost.

static bool
lvalue_p(tree exp)
{
  const enum tree_code code = TREE_CODE (exp);

  switch (code)
    {
    case SAVE_EXPR:
      return false;

    case ARRAY_REF:
    case INDIRECT_REF:
    case VAR_DECL:
    case PARM_DECL:
    case RESULT_DECL:
      return !FUNC_OR_METHOD_TYPE_P (TREE_TYPE (exp));

    case IMAGPART_EXPR:
    case REALPART_EXPR:
    case COMPONENT_REF:
    CASE_CONVERT:
      return lvalue_p(TREE_OPERAND (exp, 0));

    case COND_EXPR:
      return (lvalue_p(TREE_OPERAND (exp, 1)
                      ? TREE_OPERAND (exp, 1)
                      : TREE_OPERAND (exp, 0))
             && lvalue_p(TREE_OPERAND (exp, 2)));

    case TARGET_EXPR:
      return true;

    case COMPOUND_EXPR:
      return lvalue_p(TREE_OPERAND (exp, 1));

    default:
      return false;
    }
}

// Create a SAVE_EXPR if EXP might have unwanted side effects if referenced
// more than once in an expression.

tree
d_save_expr(tree exp)
{
  if (TREE_SIDE_EFFECTS (exp))
    {
      if (lvalue_p(exp))
	return stabilize_reference(exp);

      return save_expr(exp);
    }

  return exp;
}

// VALUEP is an expression we want to pre-evaluate or perform a computation on.
// The expression returned by this function is the part whose value we don't
// care about, storing the value in VALUEP.  Callers must ensure that the
// returned expression is evaluated before VALUEP.

tree
stabilize_expr(tree *valuep)
{
  const enum tree_code code = TREE_CODE (*valuep);
  tree lhs;
  tree rhs;

  switch (code)
    {
    case COMPOUND_EXPR:
      // Given ((e1, ...), eN): store the last RHS 'eN' expression in VALUEP.
      lhs = TREE_OPERAND (*valuep, 0);
      rhs = TREE_OPERAND (*valuep, 1);
      lhs = compound_expr(lhs, stabilize_expr(&rhs));
      *valuep = rhs;
      return lhs;

    default:
      return NULL_TREE;
    }
}

// Return a TARGET_EXPR using EXP to initialize a new temporary.

tree
build_target_expr(tree exp)
{
  tree type = TREE_TYPE (exp);
  tree slot = create_temporary_var(type);
  tree result = build4(TARGET_EXPR, type, slot, exp, NULL_TREE, NULL_TREE);

  if (EXPR_HAS_LOCATION (exp))
    SET_EXPR_LOCATION (result, EXPR_LOCATION (exp));

  // If slot must always reside in memory.
  if (TREE_ADDRESSABLE (type))
    d_mark_addressable(slot);

  // Always set TREE_SIDE_EFFECTS so that expand_expr does not ignore the
  // TARGET_EXPR.  If there really turn out to be no side effects, then the
  // optimizer should be able to remove it.
  TREE_SIDE_EFFECTS (result) = 1;

  return result;
}

// Returns the address of the expression EXP.

tree
build_address(tree exp)
{
  if (error_operand_p (exp))
    return exp;

  tree ptrtype;
  tree type = TREE_TYPE (exp);

  if (TREE_CODE (exp) == STRING_CST)
    {
      // Just convert string literals (char[]) to C-style strings (char *),
      // otherwise the latter method (char[]*) causes conversion problems
      // during gimplification.
      ptrtype = build_pointer_type(TREE_TYPE (type));
    }
  else if (TYPE_MAIN_VARIANT (type) == TYPE_MAIN_VARIANT (va_list_type_node)
	   && TREE_CODE (TYPE_MAIN_VARIANT (type)) == ARRAY_TYPE)
    {
      // Special case for va_list.  Backend will be expects a pointer to va_list,
      // but some targets use an array type.  So decay the array to pointer.
      ptrtype = build_pointer_type(TREE_TYPE (type));
    }
  else
    ptrtype = build_pointer_type(type);

  // Maybe rewrite: &(e1, e2) => (e1, &e2)
  tree init = stabilize_expr(&exp);

  // Can't take the address of a manifest constant, instead use its value.
  if (TREE_CODE (exp) == CONST_DECL)
    exp = DECL_INITIAL (exp);

  d_mark_addressable(exp);
  exp = build_fold_addr_expr_with_type_loc(input_location, exp, ptrtype);

  if (TREE_CODE (exp) == ADDR_EXPR)
    TREE_NO_TRAMPOLINE (exp) = 1;

  return compound_expr(init, exp);
}

tree
d_mark_addressable (tree exp)
{
  switch (TREE_CODE (exp))
    {
    case ADDR_EXPR:
    case COMPONENT_REF:
    case ARRAY_REF:
    case REALPART_EXPR:
    case IMAGPART_EXPR:
      d_mark_addressable(TREE_OPERAND (exp, 0));
      break;

    case PARM_DECL:
    case VAR_DECL:
    case RESULT_DECL:
    case CONST_DECL:
    case FUNCTION_DECL:
      TREE_ADDRESSABLE (exp) = 1;
      break;

    case CONSTRUCTOR:
      TREE_ADDRESSABLE (exp) = 1;
      break;

    case TARGET_EXPR:
      TREE_ADDRESSABLE (exp) = 1;
      d_mark_addressable(TREE_OPERAND (exp, 0));
      break;

    default:
      break;
    }

  return exp;
}

/* Mark EXP as "used" in the program for the benefit of
   -Wunused warning purposes.  */

tree
d_mark_used (tree exp)
{
  switch (TREE_CODE (exp))
    {
    case VAR_DECL:
    case CONST_DECL:
    case PARM_DECL:
    case RESULT_DECL:
    case FUNCTION_DECL:
      TREE_USED (exp) = 1;
      break;

    case ARRAY_REF:
    case COMPONENT_REF:
    case MODIFY_EXPR:
    case REALPART_EXPR:
    case IMAGPART_EXPR:
    case NOP_EXPR:
    case CONVERT_EXPR:
    case ADDR_EXPR:
      d_mark_used (TREE_OPERAND (exp, 0));
      break;

    case COMPOUND_EXPR:
      d_mark_used (TREE_OPERAND (exp, 0));
      d_mark_used (TREE_OPERAND (exp, 1));
      break;

    default:
      break;
    }
  return exp;
}

/* Mark EXP as read, not just set, for set but not used -Wunused
   warning purposes.  */

tree
d_mark_read (tree exp)
{
  switch (TREE_CODE (exp))
    {
    case VAR_DECL:
    case PARM_DECL:
      TREE_USED (exp) = 1;
      DECL_READ_P (exp) = 1;
      break;

    case ARRAY_REF:
    case COMPONENT_REF:
    case MODIFY_EXPR:
    case REALPART_EXPR:
    case IMAGPART_EXPR:
    case NOP_EXPR:
    case CONVERT_EXPR:
    case ADDR_EXPR:
      d_mark_read (TREE_OPERAND (exp, 0));
      break;

    case COMPOUND_EXPR:
      d_mark_read (TREE_OPERAND (exp, 1));
      break;

    default:
      break;
    }
  return exp;
}

// Return TRUE if the struct SD is suitable for comparison using memcmp.
// This is because we don't guarantee that padding is zero-initialized for
// a stack variable, so we can't use memcmp to compare struct values.

bool
identity_compare_p(StructDeclaration *sd)
{
  if (sd->isUnionDeclaration())
    return true;

  unsigned offset = 0;

  for (size_t i = 0; i < sd->fields.dim; i++)
    {
      VarDeclaration *vd = sd->fields[i];

      // Check inner data structures.
      if (vd->type->ty == Tstruct)
	{
	  TypeStruct *ts = (TypeStruct *) vd->type;
	  if (!identity_compare_p(ts->sym))
	    return false;
	}

      if (offset <= vd->offset)
	{
	  // There's a hole in the struct.
	  if (offset != vd->offset)
	    return false;

	  offset += vd->type->size();
	}
    }

  // Any trailing padding may not be zero.
  if (offset < sd->structsize)
    return false;

  return true;
}

// Lower a field-by-field equality expression between T1 and T2 of type SD.
// CODE is the EQ_EXPR or NE_EXPR comparison.

static tree
lower_struct_comparison(tree_code code, StructDeclaration *sd, tree t1, tree t2)
{
  tree_code tcode = (code == EQ_EXPR) ? TRUTH_ANDIF_EXPR : TRUTH_ORIF_EXPR;
  tree tmemcmp = NULL_TREE;

  // We can skip the compare if the structs are empty
  if (sd->fields.dim == 0)
    return build_boolop(code, integer_zero_node, integer_zero_node);

  // Let backend take care of union comparisons.
  if (sd->isUnionDeclaration())
    {
      tmemcmp = d_build_call_nary(builtin_decl_explicit(BUILT_IN_MEMCMP), 3,
				  build_address(t1), build_address(t2),
				  size_int(sd->structsize));

      return build_boolop(code, tmemcmp, integer_zero_node);
    }

  for (size_t i = 0; i < sd->fields.dim; i++)
    {
      VarDeclaration *vd = sd->fields[i];
      tree sfield = get_symbol_decl (vd);

      tree t1ref = component_ref(t1, sfield);
      tree t2ref = component_ref(t2, sfield);
      tree tcmp;

      if (vd->type->ty == Tstruct)
	{
	  // Compare inner data structures.
	  StructDeclaration *decl = ((TypeStruct *) vd->type)->sym;
	  tcmp = lower_struct_comparison(code, decl, t1ref, t2ref);
	}
      else
	{
	  tree stype = build_ctype(vd->type);
	  machine_mode mode = int_mode_for_mode(TYPE_MODE (stype));

	  if (vd->type->ty != Tvector && vd->type->isintegral())
	    {
	      // Integer comparison, no special handling required.
	      tcmp = build_boolop(code, t1ref, t2ref);
	    }
	  else if (mode != BLKmode)
	    {
	      // Compare field bits as their corresponding integer type.
	      //   *((T*) &t1) == *((T*) &t2)
	      tree tmode = lang_hooks.types.type_for_mode(mode, 1);

	      if (tmode == NULL_TREE)
		tmode = make_unsigned_type(GET_MODE_BITSIZE (mode));

	      t1ref = build_vconvert(tmode, t1ref);
	      t2ref = build_vconvert(tmode, t2ref);

	      tcmp = build_boolop(code, t1ref, t2ref);
	    }
	  else
	    {
	      // Simple memcmp between types.
	      tcmp = d_build_call_nary(builtin_decl_explicit(BUILT_IN_MEMCMP), 3,
				       build_address(t1ref), build_address(t2ref),
				       TYPE_SIZE_UNIT (stype));

	      tcmp = build_boolop(code, tcmp, integer_zero_node);
	    }
	}

      tmemcmp = (tmemcmp) ? build_boolop(tcode, tmemcmp, tcmp) : tcmp;
    }

  return tmemcmp;
}


// Build an equality expression between two RECORD_TYPES T1 and T2 of type SD.
// If possible, use memcmp, otherwise field-by-field comparison is done.
// CODE is the EQ_EXPR or NE_EXPR comparison.

tree
build_struct_comparison(tree_code code, StructDeclaration *sd, tree t1, tree t2)
{
  // We can skip the compare if the structs are empty
  if (sd->fields.dim == 0)
    return build_boolop(code, integer_zero_node, integer_zero_node);

  // Make temporaries to prevent multiple evaluations.
  tree t1init = stabilize_expr(&t1);
  tree t2init = stabilize_expr(&t2);
  tree result;

  t1 = d_save_expr(t1);
  t2 = d_save_expr(t2);

  // Bitwise comparison of structs not returned in memory may not work
  // due to data holes loosing its zero padding upon return.
  // As a heuristic, small structs are not compared using memcmp either.
  if (TYPE_MODE (TREE_TYPE (t1)) != BLKmode || !identity_compare_p(sd))
    result = lower_struct_comparison(code, sd, t1, t2);
  else
    {
      // Do bit compare of structs.
      tree size = size_int(sd->structsize);
      tree tmemcmp = d_build_call_nary(builtin_decl_explicit(BUILT_IN_MEMCMP), 3,
				       build_address(t1), build_address(t2), size);

      result = build_boolop(code, tmemcmp, integer_zero_node);
    }

  return compound_expr(compound_expr(t1init, t2init), result);
}

// Build an equality expression between two ARRAY_TYPES of size LENGTH.
// The pointer references are T1 and T2, and the element type is SD.
// CODE is the EQ_EXPR or NE_EXPR comparison.

tree
build_array_struct_comparison(tree_code code, StructDeclaration *sd,
			      tree length, tree t1, tree t2)
{
  tree_code tcode = (code == EQ_EXPR) ? TRUTH_ANDIF_EXPR : TRUTH_ORIF_EXPR;

  // Build temporary for the result of the comparison.
  // Initialize as either 0 or 1 depending on operation.
  tree result = build_local_temp(bool_type_node);
  tree init = build_boolop(code, integer_zero_node, integer_zero_node);
  add_stmt(build_assign(INIT_EXPR, result, init));

  // Cast pointer-to-array to pointer-to-struct.
  tree ptrtype = build_ctype(sd->type->pointerTo());
  tree lentype = TREE_TYPE (length);

  push_binding_level(level_block);
  push_stmt_list();

  // Build temporary locals for length and pointers.
  tree t = build_local_temp(size_type_node);
  add_stmt(build_assign(INIT_EXPR, t, length));
  length = t;

  t = build_local_temp(ptrtype);
  add_stmt(build_assign(INIT_EXPR, t, d_convert(ptrtype, t1)));
  t1 = t;

  t = build_local_temp(ptrtype);
  add_stmt(build_assign(INIT_EXPR, t, d_convert(ptrtype, t2)));
  t2 = t;

  // Build loop for comparing each element.
  push_stmt_list();

  // Exit logic for the loop.
  //	if (length == 0 || result OP 0) break
  t = build_boolop(EQ_EXPR, length, d_convert(lentype, integer_zero_node));
  t = build_boolop(TRUTH_ORIF_EXPR, t, build_boolop(code, result, boolean_false_node));
  t = build1(EXIT_EXPR, void_type_node, t);
  add_stmt(t);

  // Do comparison, caching the value.
  //	result = result OP (*t1 == *t2)
  t = build_struct_comparison(code, sd, build_deref(t1), build_deref(t2));
  t = build_boolop(tcode, result, t);
  t = modify_expr(result, t);
  add_stmt(t);

  // Move both pointers to next element position.
  //	t1++, t2++;
  tree size = d_convert(ptrtype, TYPE_SIZE_UNIT (TREE_TYPE (ptrtype)));
  t = build2(POSTINCREMENT_EXPR, ptrtype, t1, size);
  add_stmt(t);
  t = build2(POSTINCREMENT_EXPR, ptrtype, t2, size);
  add_stmt(t);

  // Decrease loop counter.
  //	length -= 1
  t = build2(POSTDECREMENT_EXPR, lentype, length,
	     d_convert(lentype, integer_one_node));
  add_stmt(t);

  // Pop statements and finish loop.
  tree body = pop_stmt_list();
  add_stmt(build1(LOOP_EXPR, void_type_node, body));

  // Wrap it up into a bind expression.
  tree stmt_list = pop_stmt_list();
  tree block = pop_binding_level();

  body = build3(BIND_EXPR, void_type_node,
		BLOCK_VARS (block), stmt_list, block);

  return compound_expr(body, result);
}

// Create an anonymous field of type ubyte[T] at OFFSET to fill
// the alignment hole between OFFSET and FIELDPOS.

static tree
build_alignment_field(HOST_WIDE_INT offset, HOST_WIDE_INT fieldpos)
{
  tree type = make_array_type (Type::tuns8, fieldpos - offset);
  tree field = create_field_decl(type, NULL, 1, 1);

  SET_DECL_OFFSET_ALIGN (field, TYPE_ALIGN (type));
  DECL_FIELD_OFFSET (field) = size_int(offset);
  DECL_FIELD_BIT_OFFSET (field) = bitsize_zero_node;

  layout_decl(field, 0);

  return field;
}

// Build a constructor for a variable of aggregate type TYPE using the
// initializer INIT, an ordered flat list of fields and values provided
// by the frontend.
// The returned constructor should be a value that matches the layout of TYPE.

tree
build_struct_literal(tree type, vec<constructor_elt, va_gc> *init)
{
  // If the initializer was empty, use default zero initialization.
  if (vec_safe_is_empty(init))
    return build_constructor(type, NULL);

  vec<constructor_elt, va_gc> *ve = NULL;
  HOST_WIDE_INT offset = 0;
  bool constant_p = true;
  bool fillholes = true;
  bool finished = false;

  // Filling alignment holes this only applies to structs.
  if (TREE_CODE (type) != RECORD_TYPE
      || CLASS_TYPE_P (type) || TYPE_PACKED (type))
    fillholes = false;

  // Walk through each field, matching our initializer list
  for (tree field = TYPE_FIELDS (type); field; field = DECL_CHAIN (field))
    {
      bool is_initialized = false;
      tree value;

      if (DECL_NAME (field) == NULL_TREE
	  && RECORD_OR_UNION_TYPE_P (TREE_TYPE (field))
	  && ANON_AGGR_TYPE_P (TREE_TYPE (field)))
	{
	  // Search all nesting aggregates, if nothing is found, then
	  // this will return an empty initializer to fill the hole.
	  value = build_struct_literal(TREE_TYPE (field), init);

	  if (!initializer_zerop(value))
	    is_initialized = true;
	}
      else
	{
	  // Search for the value to initialize the next field.  Once found,
	  // pop it from the init list so we don't look at it again.
	  unsigned HOST_WIDE_INT idx;
	  tree index;

	  FOR_EACH_CONSTRUCTOR_ELT (init, idx, index, value)
	    {
	      /* If the index is NULL, then just assign it to the next field.
		 This is expect of layout_typeinfo(), which generates a flat
		 list of values that we must shape into the record type.  */
	      if (index == field || index == NULL_TREE)
		{
		  init->ordered_remove(idx);
		  if (!finished)
		    is_initialized = true;
		  break;
		}
	    }
	}

      if (is_initialized)
	{
	  HOST_WIDE_INT fieldpos = int_byte_position(field);
	  gcc_assert(value != NULL_TREE);

	  // Insert anonymous fields in the constructor for padding out
	  // alignment holes in-place between fields.
	  if (fillholes && offset < fieldpos)
	    {
	      tree pfield = build_alignment_field(offset, fieldpos);
	      tree pvalue = build_constructor(TREE_TYPE (pfield), NULL);
	      CONSTRUCTOR_APPEND_ELT (ve, pfield, pvalue);
	    }

	  // Must not initialize fields that overlap.
	  if (fieldpos < offset)
	    {
	      // Find the nearest user defined type and field.
	      tree vtype = type;
	      while (ANON_AGGR_TYPE_P (vtype))
		vtype = TYPE_CONTEXT (vtype);

	      tree vfield = field;
	      if (RECORD_OR_UNION_TYPE_P (TREE_TYPE (vfield))
		  && ANON_AGGR_TYPE_P (TREE_TYPE (vfield)))
		vfield = TYPE_FIELDS (TREE_TYPE (vfield));

	      // Must not generate errors for compiler generated fields.
	      gcc_assert(TYPE_NAME (vtype) && DECL_NAME (vfield));
	      error("overlapping initializer for field %qT.%qD",
		    TYPE_NAME (vtype), DECL_NAME (vfield));
	    }

	  if (!TREE_CONSTANT (value))
	    constant_p = false;

	  CONSTRUCTOR_APPEND_ELT (ve, field, value);

	  // For unions, only the first field is initialized, any other
	  // field initializers found for this union are drained and ignored.
	  if (TREE_CODE (type) == UNION_TYPE)
	    finished = true;
	}

      // Move offset to the next position in the struct.
      if (TREE_CODE (type) == RECORD_TYPE)
	offset = int_byte_position(field) + int_size_in_bytes(TREE_TYPE (field));

      // If all initializers have been assigned, there's nothing else to do.
      if (vec_safe_is_empty(init))
	break;
    }

  // Finally pad out the end of the record.
  if (fillholes && offset < int_size_in_bytes(type))
    {
      tree pfield = build_alignment_field(offset, int_size_in_bytes(type));
      tree pvalue = build_constructor(TREE_TYPE (pfield), NULL);
      CONSTRUCTOR_APPEND_ELT (ve, pfield, pvalue);
    }

  // Ensure that we have consumed all values.
  gcc_assert(vec_safe_is_empty(init) || ANON_AGGR_TYPE_P (type));

  tree ctor = build_constructor(type, ve);

  if (constant_p)
    TREE_CONSTANT (ctor) = 1;

  return ctor;
}

// Return a constructor that matches the layout of the class expression EXP.

tree
build_class_instance (ClassReferenceExp *exp)
{
  ClassDeclaration *cd = exp->originalClass ();
  tree type = TREE_TYPE (build_ctype (exp->value->stype));
  vec<constructor_elt, va_gc> *ve = NULL;

  // The set base vtable field.
  tree vptr = build_address (get_vtable_decl (cd));
  CONSTRUCTOR_APPEND_ELT (ve, TYPE_FIELDS (type), vptr);

  // Go through the inheritance graph from top to bottom.  This will add all
  // values to the constructor out of order, however build_struct_literal
  // will re-order all values before returning the finished literal.
  for (ClassDeclaration *bcd = cd; bcd != NULL; bcd = bcd->baseClass)
    {
      // Anonymous vtable interface fields are layed out before the fields of
      // each class.  The interface offset is used to determine where to put
      // the classinfo offset reference.
      for (size_t i = 0; i < bcd->vtblInterfaces->dim; i++)
	{
	  BaseClass *bc = (*bcd->vtblInterfaces)[i];

	  for (ClassDeclaration *cd2 = cd; 1; cd2 = cd2->baseClass)
	    {
	      gcc_assert (cd2 != NULL);

	      unsigned csymoffset = base_vtable_offset (cd2, bc);
	      if (csymoffset != (unsigned) ~0)
		{
		  tree csym = build_address (get_classinfo_decl (cd2));
		  csym = build_offset (csym, size_int (csymoffset));

		  tree field = find_aggregate_field (type, NULL_TREE,
						     size_int (bc->offset));
		  gcc_assert (field != NULL_TREE);

		  CONSTRUCTOR_APPEND_ELT (ve, field, csym);
		  break;
		}
	    }
	}

      // Generate initial values of all fields owned by current class.
      // Use both the name and offset to find the right field.
      for (size_t i = 0; i < bcd->fields.dim; i++)
	{
	  VarDeclaration *vfield = bcd->fields[i];
	  int index = exp->findFieldIndexByName (vfield);
	  gcc_assert (index != -1);

	  Expression *value = (*exp->value->elements)[index];
	  if (!value)
	    continue;

	  // Use find_aggregate_field to get the overridden field decl,
	  // instead of the field associated with the base class.
	  tree field = get_symbol_decl (bcd->fields[i]);
	  field = find_aggregate_field (type, DECL_NAME (field),
					DECL_FIELD_OFFSET (field));
	  gcc_assert (field != NULL_TREE);

	  CONSTRUCTOR_APPEND_ELT (ve, field, build_expr (value, true));
	}
    }

  return build_struct_literal (type, ve);
}

// Given the TYPE of an anonymous field inside T, return the
// FIELD_DECL for the field.  If not found return NULL_TREE.
// Because anonymous types can nest, we must also search all
// anonymous fields that are directly reachable.

static tree
lookup_anon_field(tree t, tree type)
{
  t = TYPE_MAIN_VARIANT (t);

  for (tree field = TYPE_FIELDS (t); field; field = DECL_CHAIN (field))
    {
      if (DECL_NAME (field) == NULL_TREE)
	{
	  // If we find it directly, return the field.
	  if (type == TYPE_MAIN_VARIANT (TREE_TYPE (field)))
	    return field;

	  // Otherwise, it could be nested, search harder.
	  if (RECORD_OR_UNION_TYPE_P (TREE_TYPE (field))
	      && ANON_AGGR_TYPE_P (TREE_TYPE (field)))
	    {
	      tree subfield = lookup_anon_field(TREE_TYPE (field), type);
	      if (subfield)
		return subfield;
	    }
	}
    }

  return NULL_TREE;
}

// Builds OBJECT.FIELD component reference.

tree
component_ref(tree object, tree field)
{
  if (error_operand_p(object) || error_operand_p(field))
    return error_mark_node;

  gcc_assert (TREE_CODE (field) == FIELD_DECL);

  // Maybe rewrite: (e1, e2).field => (e1, e2.field)
  tree init = stabilize_expr(&object);

  // If the FIELD is from an anonymous aggregate, generate a reference
  // to the anonymous data member, and recur to find FIELD.
  if (ANON_AGGR_TYPE_P (DECL_CONTEXT (field)))
    {
      tree anonymous_field = lookup_anon_field(TREE_TYPE (object), DECL_CONTEXT (field));
      object = component_ref(object, anonymous_field);
    }

  tree result = fold_build3_loc(input_location, COMPONENT_REF,
				TREE_TYPE (field), object, field, NULL_TREE);

  return compound_expr(init, result);
}

// Build an assignment expression of lvalue LHS from value RHS.
// CODE is the code for a binary operator that we use to combine
// the old value of LHS with RHS to get the new value.

tree
build_assign(tree_code code, tree lhs, tree rhs)
{
  tree init = stabilize_expr(&lhs);
  init = compound_expr(init, stabilize_expr(&rhs));

  // If initializing the LHS using a function that returns via NRVO.
  if (code == INIT_EXPR && TREE_CODE (rhs) == CALL_EXPR
      && AGGREGATE_TYPE_P (TREE_TYPE (rhs))
      && aggregate_value_p (TREE_TYPE (rhs), rhs))
    {
      // Mark as addressable here, which should ensure the return slot is the
      // address of the LHS expression, taken care of by back-end.
      d_mark_addressable (lhs);
      CALL_EXPR_RETURN_SLOT_OPT (rhs) = true;
    }

  // The LHS assignment replaces the temporary in TARGET_EXPR_SLOT.
  if (TREE_CODE (rhs) == TARGET_EXPR)
    {
      // If CODE is not INIT_EXPR, can't initialize LHS directly,
      // since that would cause the LHS to be constructed twice.
      // So we force the TARGET_EXPR to be expanded without a target.
      if (code != INIT_EXPR)
	rhs = compound_expr(rhs, TARGET_EXPR_SLOT (rhs));
      else
	{
	  d_mark_addressable (lhs);
	  rhs = TARGET_EXPR_INITIAL (rhs);
	}
    }

  tree result = fold_build2_loc(input_location, code,
				TREE_TYPE (lhs), lhs, rhs);
  return compound_expr(init, result);
}

tree
modify_expr(tree lhs, tree rhs)
{
  return build_assign(MODIFY_EXPR, lhs, rhs);
}

// Return EXP represented as TYPE.

tree
build_nop(tree type, tree exp)
{
  if (error_operand_p (exp))
    return exp;

  // Maybe rewrite: (e1, e2) => (e1, cast(TYPE) e2)
  tree init = stabilize_expr(&exp);
  exp = fold_build1_loc(input_location, NOP_EXPR, type, exp);

  return compound_expr(init, exp);
}

tree
build_vconvert(tree type, tree exp)
{
  return indirect_ref(type, build_address(exp));
}

// Build a boolean ARG0 op ARG1 expression.

tree
build_boolop(tree_code code, tree arg0, tree arg1)
{
  // Aggregate comparisons may get lowered to a call to builtin memcmp,
  // so need to remove all side effects incase it's address is taken.
  if (AGGREGATE_TYPE_P (TREE_TYPE (arg0)))
    arg0 = d_save_expr(arg0);
  if (AGGREGATE_TYPE_P (TREE_TYPE (arg1)))
    arg1 = d_save_expr(arg1);

  return fold_build2_loc(input_location, code, bool_type_node,
			 arg0, d_convert(TREE_TYPE (arg0), arg1));
}

// Return a COND_EXPR.  ARG0, ARG1, and ARG2 are the three
// arguments to the conditional expression.

tree
build_condition(tree type, tree arg0, tree arg1, tree arg2)
{
  if (arg1 == void_node)
    arg1 = build_empty_stmt(input_location);

  if (arg2 == void_node)
    arg2 = build_empty_stmt(input_location);

  return fold_build3_loc(input_location, COND_EXPR,
			 type, arg0, arg1, arg2);
}

tree
build_vcondition(tree arg0, tree arg1, tree arg2)
{
  return build_condition(void_type_node, arg0, arg1, arg2);
}

// Build a compound expr to join ARG0 and ARG1 together.

tree
compound_expr(tree arg0, tree arg1)
{
  if (arg1 == NULL_TREE)
    return arg0;

  if (arg0 == NULL_TREE || !TREE_SIDE_EFFECTS (arg0))
    return arg1;

  if (TREE_CODE (arg1) == TARGET_EXPR)
    {
      // If the rhs is a TARGET_EXPR, then build the compound expression
      // inside the target_expr's initializer.  This helps the compiler
      // to eliminate unnecessary temporaries.
      tree init = compound_expr(arg0, TARGET_EXPR_INITIAL (arg1));
      TARGET_EXPR_INITIAL (arg1) = init;

      return arg1;
    }

  return fold_build2_loc(input_location, COMPOUND_EXPR,
			 TREE_TYPE (arg1), arg0, arg1);
}

// Build a return expression.

tree
return_expr(tree ret)
{
  return fold_build1_loc(input_location, RETURN_EXPR,
			 void_type_node, ret);
}

//

tree
size_mult_expr(tree arg0, tree arg1)
{
  return fold_build2_loc(input_location, MULT_EXPR, size_type_node,
			 d_convert(size_type_node, arg0),
			 d_convert(size_type_node, arg1));

}

// Return the real part of CE, which should be a complex expression.

tree
real_part(tree ce)
{
  return fold_build1_loc(input_location, REALPART_EXPR,
			 TREE_TYPE (TREE_TYPE (ce)), ce);
}

// Return the imaginary part of CE, which should be a complex expression.

tree
imaginary_part(tree ce)
{
  return fold_build1_loc(input_location, IMAGPART_EXPR,
			 TREE_TYPE (TREE_TYPE (ce)), ce);
}

// Build a complex expression of type TYPE using RE and IM.

tree
complex_expr(tree type, tree re, tree im)
{
  return fold_build2_loc(input_location, COMPLEX_EXPR,
			 type, re, im);
}

// Cast EXP (which should be a pointer) to TYPE * and then indirect.  The
// back-end requires this cast in many cases.

tree
indirect_ref(tree type, tree exp)
{
  if (error_operand_p (exp))
    return exp;

  // Maybe rewrite: (e1, e2) => (e1, *e2)
  tree init = stabilize_expr(&exp);

  if (TREE_CODE (TREE_TYPE (exp)) == REFERENCE_TYPE)
    exp = fold_build1(INDIRECT_REF, type, exp);
  else
    {
      exp = build_nop(build_pointer_type(type), exp);
      exp = build_deref(exp);
    }

  return compound_expr(init, exp);
}

// Returns indirect reference of EXP, which must be a pointer type.

tree
build_deref(tree exp)
{
  if (error_operand_p (exp))
    return exp;

  // Maybe rewrite: (e1, e2) => (e1, *e2)
  tree init = stabilize_expr(&exp);

  gcc_assert(POINTER_TYPE_P (TREE_TYPE (exp)));

  if (TREE_CODE (exp) == ADDR_EXPR)
    exp = TREE_OPERAND (exp, 0);
  else
    exp = build_fold_indirect_ref(exp);

  return compound_expr(init, exp);
}

// Builds pointer offset expression PTR[INDEX]

tree
build_array_index(tree ptr, tree index)
{
  if (error_operand_p(ptr) || error_operand_p(index))
    return error_mark_node;

  tree ptr_type = TREE_TYPE (ptr);
  tree target_type = TREE_TYPE (ptr_type);

  tree type = lang_hooks.types.type_for_size(TYPE_PRECISION (sizetype),
					     TYPE_UNSIGNED (sizetype));

  // array element size
  tree size_exp = size_in_bytes(target_type);

  if (integer_zerop(size_exp))
    {
      // Test for void case...
      if (TYPE_MODE (target_type) == TYPE_MODE (void_type_node))
	index = fold_convert(type, index);
      else
	{
	  // FIXME: should catch this earlier.
	  error("invalid use of incomplete type %qD", TYPE_NAME (target_type));
	  ptr_type = error_mark_node;
	}
    }
  else if (integer_onep(size_exp))
    {
      // ...or byte case -- No need to multiply.
      index = fold_convert(type, index);
    }
  else
    {
      index = d_convert(type, index);
      index = fold_build2(MULT_EXPR, TREE_TYPE (index),
			  index, d_convert(TREE_TYPE (index), size_exp));
      index = fold_convert(type, index);
    }

  if (integer_zerop(index))
    return ptr;

  return fold_build2(POINTER_PLUS_EXPR, ptr_type, ptr, index);
}

// Builds pointer offset expression *(PTR OP INDEX)
// OP could be a plus or minus expression.

tree
build_offset_op(tree_code op, tree ptr, tree index)
{
  gcc_assert(op == MINUS_EXPR || op == PLUS_EXPR);

  tree type = lang_hooks.types.type_for_size(TYPE_PRECISION (sizetype),
					     TYPE_UNSIGNED (sizetype));
  index = fold_convert(type, index);

  if (op == MINUS_EXPR)
    index = fold_build1(NEGATE_EXPR, type, index);

  return fold_build2(POINTER_PLUS_EXPR, TREE_TYPE (ptr), ptr, index);
}

tree
build_offset(tree ptr_node, tree byte_offset)
{
  tree ofs = fold_convert(build_ctype(Type::tsize_t), byte_offset);
  return fold_build2(POINTER_PLUS_EXPR, TREE_TYPE (ptr_node), ptr_node, ofs);
}

tree
build_memref(tree type, tree ptr, tree byte_offset)
{
  tree ofs = fold_convert(type, byte_offset);
  return fold_build2(MEM_REF, type, ptr, ofs);
}


// Create a tree node to set multiple elements to a single value

tree
build_array_set(tree ptr, tree length, tree value)
{
  tree ptrtype = TREE_TYPE (ptr);
  tree lentype = TREE_TYPE (length);

  push_binding_level(level_block);
  push_stmt_list();

  // Build temporary locals for length and ptr, and maybe value.
  tree t = build_local_temp(size_type_node);
  add_stmt(build_assign(INIT_EXPR, t, length));
  length = t;

  t = build_local_temp(ptrtype);
  add_stmt(build_assign(INIT_EXPR, t, ptr));
  ptr = t;

  if (TREE_SIDE_EFFECTS (value))
    {
      t = build_local_temp(TREE_TYPE (value));
      add_stmt(build_assign(INIT_EXPR, t, value));
      value = t;
    }

  // Build loop to initialise { .length=length, .ptr=ptr } with value.
  push_stmt_list();

  // Exit logic for the loop.
  //	if (length == 0) break
  t = build_boolop(EQ_EXPR, length, d_convert(lentype, integer_zero_node));
  t = build1(EXIT_EXPR, void_type_node, t);
  add_stmt(t);

  // Assign value to the current pointer position.
  //	*ptr = value
  t = modify_expr(build_deref(ptr), value);
  add_stmt(t);

  // Move pointer to next element position.
  //	ptr++;
  tree size = TYPE_SIZE_UNIT (TREE_TYPE (ptrtype));
  t = build2(POSTINCREMENT_EXPR, ptrtype, ptr, d_convert(ptrtype, size));
  add_stmt(t);

  // Decrease loop counter.
  //	length -= 1
  t = build2(POSTDECREMENT_EXPR, lentype, length,
	     d_convert(lentype, integer_one_node));
  add_stmt(t);

  // Pop statements and finish loop.
  tree loop_body = pop_stmt_list();
  add_stmt(build1(LOOP_EXPR, void_type_node, loop_body));

  // Wrap it up into a bind expression.
  tree stmt_list = pop_stmt_list();
  tree block = pop_binding_level();

  return build3(BIND_EXPR, void_type_node,
		BLOCK_VARS (block), stmt_list, block);
}


// Build an array of type TYPE where all the elements are VAL.

tree
build_array_from_val(Type *type, tree val)
{
  gcc_assert(type->ty == Tsarray);

  tree etype = build_ctype(type->nextOf());

  // Initializing a multidimensional array.
  if (TREE_CODE (etype) == ARRAY_TYPE && TREE_TYPE (val) != etype)
    val = build_array_from_val(type->nextOf(), val);

  size_t dims = ((TypeSArray *) type)->dim->toInteger();
  vec<constructor_elt, va_gc> *elms = NULL;
  vec_safe_reserve(elms, dims);

  val = d_convert(etype, val);

  for (size_t i = 0; i < dims; i++)
    CONSTRUCTOR_APPEND_ELT (elms, size_int(i), val);

  return build_constructor(build_ctype(type), elms);
}

// Implicitly converts void* T to byte* as D allows { void[] a; &a[3]; }

tree
void_okay_p(tree t)
{
  tree type = TREE_TYPE (t);

  if (VOID_TYPE_P (TREE_TYPE (type)))
    {
      tree totype = build_ctype(Type::tuns8->pointerTo());
      return fold_convert(totype, t);
    }

  return t;
}

// Build an expression of code CODE, data type TYPE, and operands ARG0
// and ARG1. Perform relevant conversions needs for correct code operations.

tree
build_binary_op(tree_code code, tree type, tree arg0, tree arg1)
{
  tree t0 = TREE_TYPE (arg0);
  tree t1 = TREE_TYPE (arg1);
  tree ret = NULL_TREE;

  bool unsignedp = TYPE_UNSIGNED (t0) || TYPE_UNSIGNED (t1);

  // Deal with float mod expressions immediately.
  if (code == FLOAT_MOD_EXPR)
    return build_float_modulus(TREE_TYPE (arg0), arg0, arg1);

  if (POINTER_TYPE_P (t0) && INTEGRAL_TYPE_P (t1))
    return build_nop(type, build_offset_op(code, arg0, arg1));

  if (INTEGRAL_TYPE_P (t0) && POINTER_TYPE_P (t1))
    return build_nop(type, build_offset_op(code, arg1, arg0));

  if (POINTER_TYPE_P (t0) && POINTER_TYPE_P (t1))
    {
      // Need to convert pointers to integers because tree-vrp asserts
      // against (ptr MINUS ptr).
      tree ptrtype = lang_hooks.types.type_for_mode(ptr_mode, TYPE_UNSIGNED (type));
      arg0 = d_convert(ptrtype, arg0);
      arg1 = d_convert(ptrtype, arg1);

      ret = fold_build2(code, ptrtype, arg0, arg1);
    }
  else if (INTEGRAL_TYPE_P (type) && (TYPE_UNSIGNED (type) != unsignedp))
    {
      tree inttype = unsignedp ? d_unsigned_type(type) : d_signed_type(type);
      ret = fold_build2(code, inttype, arg0, arg1);
    }
  else
    {
      // Front-end does not do this conversion and GCC does not
      // always do it right.
      if (COMPLEX_FLOAT_TYPE_P (t0) && !COMPLEX_FLOAT_TYPE_P (t1))
	arg1 = d_convert(t0, arg1);
      else if (COMPLEX_FLOAT_TYPE_P (t1) && !COMPLEX_FLOAT_TYPE_P (t0))
	arg0 = d_convert(t1, arg0);

      ret = fold_build2(code, type, arg0, arg1);
    }

  return d_convert(type, ret);
}

// Build a binary expression of code CODE, assigning the result into E1.

tree
build_binop_assignment(tree_code code, Expression *e1, Expression *e2)
{
  // Skip casts for lhs assignment.
  Expression *e1b = e1;
  while (e1b->op == TOKcast)
    {
      CastExp *ce = (CastExp *) e1b;
      gcc_assert (same_type_p (ce->type, ce->to));
      e1b = ce->e1;
    }

  // The LHS expression could be an assignment, to which it's operation gets
  // lost during gimplification.  Stabilize lhs for assignment.
  tree lhs = build_expr(e1b);
  tree lexpr = stabilize_expr(&lhs);

  lhs = stabilize_reference(lhs);

  tree rhs = build_expr(e2);
  rhs = build_binary_op(code, build_ctype(e1->type),
			convert_expr(lhs, e1b->type, e1->type), rhs);

  tree expr = modify_expr(lhs, convert_expr(rhs, e1->type, e1b->type));

  return compound_expr(lexpr, expr);
}

// Builds a bounds condition checking that INDEX is between 0 and LEN.
// The condition returns the INDEX if true, or throws a RangeError.
// If INCLUSIVE, we allow INDEX == LEN to return true also.

tree
build_bounds_condition(const Loc& loc, tree index, tree len, bool inclusive)
{
  if (!array_bounds_check())
    return index;

  // Prevent multiple evaluations of the index.
  index = d_save_expr(index);

  // Generate INDEX >= LEN && throw RangeError.
  // No need to check whether INDEX >= 0 as the front-end should
  // have already taken care of implicit casts to unsigned.
  tree condition = fold_build2(inclusive ? GT_EXPR : GE_EXPR,
			       bool_type_node, index, len);
  tree boundserr = d_assert_call(loc, LIBCALL_ARRAY_BOUNDS);

  return build_condition(TREE_TYPE (index), condition, boundserr, index);
}

// Returns TRUE if array bounds checking code generation is turned on.

bool
array_bounds_check()
{
  FuncDeclaration *func;

  switch (global.params.useArrayBounds)
    {
    case BOUNDSCHECKoff:
      return false;

    case BOUNDSCHECKon:
      return true;

    case BOUNDSCHECKsafeonly:
      // For D2 safe functions only
      func = d_function_chain->function;
      if (func && func->type->ty == Tfunction)
	{
	  TypeFunction *tf = (TypeFunction *) func->type;
	  if (tf->trust == TRUSTsafe)
	    return true;
	}
      return false;

    default:
      gcc_unreachable();
    }
}

/* Return an undeclared local temporary of type TYPE
   for use with BIND_EXPR.  */

tree
create_temporary_var (tree type)
{
  tree decl = build_decl (input_location, VAR_DECL, NULL_TREE, type);

  DECL_CONTEXT (decl) = current_function_decl;
  DECL_ARTIFICIAL (decl) = 1;
  DECL_IGNORED_P (decl) = 1;
  layout_decl (decl, 0);

  return decl;
}

/* Return an undeclared local temporary OUT_VAR initialised
   with result of expression EXP.  */

tree
maybe_temporary_var (tree exp, tree *out_var)
{
  tree t = exp;

  /* Get the base component.  */
  while (TREE_CODE (t) == COMPONENT_REF)
    t = TREE_OPERAND (t, 0);

  if (!DECL_P (t) && !REFERENCE_CLASS_P (t))
    {
      *out_var = create_temporary_var (TREE_TYPE (exp));
      DECL_INITIAL (*out_var) = exp;
      return *out_var;
    }
  else
    {
      *out_var = NULL_TREE;
      return exp;
    }
}

// Builds a BIND_EXPR around BODY for the variables VAR_CHAIN.

tree
bind_expr (tree var_chain, tree body)
{
  // TODO: only handles one var
  gcc_assert (TREE_CHAIN (var_chain) == NULL_TREE);

  if (DECL_INITIAL (var_chain))
    {
      tree ini = build_assign(INIT_EXPR, var_chain, DECL_INITIAL (var_chain));
      DECL_INITIAL (var_chain) = NULL_TREE;
      body = compound_expr (ini, body);
    }

  return d_save_expr(build3 (BIND_EXPR, TREE_TYPE (body), var_chain, body, NULL_TREE));
}

// Returns the TypeFunction class for Type T.
// Assumes T is already ->toBasetype()

TypeFunction *
get_function_type (Type *t)
{
  TypeFunction *tf = NULL;
  if (t->ty == Tpointer)
    t = t->nextOf()->toBasetype();
  if (t->ty == Tfunction)
    tf = (TypeFunction *) t;
  else if (t->ty == Tdelegate)
    tf = (TypeFunction *) ((TypeDelegate *) t)->next;
  return tf;
}

// Returns TRUE if CALLEE is a plain nested function outside the scope of CALLER.
// In which case, CALLEE is being called through an alias that was passed to CALLER.

bool
call_by_alias_p (FuncDeclaration *caller, FuncDeclaration *callee)
{
  if (!callee->isNested())
    return false;

  if (caller->toParent() == callee->toParent())
    return false;

  Dsymbol *dsym = callee;

  while (dsym)
    {
      if (dsym->isTemplateInstance())
	return false;
      else if (dsym->isFuncDeclaration() == caller)
	return false;
      dsym = dsym->toParent();
    }

  return true;
}

// Entry point for call routines.  Builds a function call to FD.
// OBJECT is the 'this' reference passed and ARGS are the arguments to FD.

tree
d_build_call(FuncDeclaration *fd, tree object, Expressions *args)
{
  return d_build_call(get_function_type(fd->type),
		      build_address(get_symbol_decl (fd)), object, args);
}

// Builds a CALL_EXPR of type TF to CALLABLE. OBJECT holds the 'this' pointer,
// ARGUMENTS are evaluated in left to right order, saved and promoted before passing.

tree
d_build_call(TypeFunction *tf, tree callable, tree object, Expressions *arguments)
{
  tree ctype = TREE_TYPE (callable);
  tree callee = callable;

  if (POINTER_TYPE_P (ctype))
    ctype = TREE_TYPE (ctype);
  else
    callee = build_address(callable);

  gcc_assert(FUNC_OR_METHOD_TYPE_P (ctype));
  gcc_assert(tf != NULL);
  gcc_assert(tf->ty == Tfunction);

  if (TREE_CODE (ctype) != FUNCTION_TYPE && object == NULL_TREE)
    {
      // Front-end apparently doesn't check this.
      if (TREE_CODE (callable) == FUNCTION_DECL)
	{
	  error("need 'this' to access member %s",
		IDENTIFIER_POINTER (DECL_NAME (callable)));
	  return error_mark_node;
	}

      // Probably an internal error
      gcc_unreachable();
    }

  // Build the argument list for the call.
  tree arg_list = NULL_TREE;
  tree saved_args = NULL_TREE;

  // If this is a delegate call or a nested function being called as
  // a delegate, the object should not be NULL.
  if (object != NULL_TREE)
    arg_list = build_tree_list(NULL_TREE, object);

  if (arguments)
    {
      // First pass, evaluated expanded tuples in function arguments.
      for (size_t i = 0; i < arguments->dim; ++i)
	{
	Lagain:
	  Expression *arg = (*arguments)[i];
	  gcc_assert(arg->op != TOKtuple);

	  if (arg->op == TOKcomma)
	    {
	      CommaExp *ce = (CommaExp *) arg;
	      tree tce = build_expr(ce->e1);
	      saved_args = compound_expr(saved_args, tce);
	      (*arguments)[i] = ce->e2;
	      goto Lagain;
	    }
	}

      // if _arguments[] is the first argument.
      size_t dvarargs = (tf->linkage == LINKd && tf->varargs == 1);
      size_t nparams = Parameter::dim(tf->parameters);

      // Assumes arguments->dim <= formal_args->dim if (!this->varargs)
      for (size_t i = 0; i < arguments->dim; ++i)
	{
	  Expression *arg = (*arguments)[i];
	  tree targ;

	  if (i < dvarargs)
	    {
	      // The hidden _arguments parameter
	      targ = build_expr(arg);
	    }
	  else if (i - dvarargs < nparams && i >= dvarargs)
	    {
	      // Actual arguments for declared formal arguments
	      Parameter *parg = Parameter::getNth(tf->parameters, i - dvarargs);
	      targ = convert_for_argument(build_expr(arg), parg);
	    }
	  else
	    targ = build_expr(arg);

	  // Don't pass empty aggregates by value.
	  if (empty_aggregate_p(TREE_TYPE (targ)) && !TREE_ADDRESSABLE (targ)
	      && TREE_CODE (targ) != CONSTRUCTOR)
	    {
	      tree t = build_constructor(TREE_TYPE (targ), NULL);
	      targ = build2(COMPOUND_EXPR, TREE_TYPE (t), targ, t);
	    }

	  arg_list = chainon(arg_list, build_tree_list(0, targ));
	}
    }

  // Evaluate the argument before passing to the function.
  // Needed for left to right evaluation.
  if (tf->linkage == LINKd)
    {
      size_t nsaved_args = 0;

      // First, push left side of comma expressions into the saved args list,
      // so that only the result is maybe made a temporary.
      for (tree arg = arg_list; arg != NULL_TREE; arg = TREE_CHAIN (arg))
	{
	  tree value = TREE_VALUE (arg);

	  if (TREE_SIDE_EFFECTS (value))
	    {
	      tree expr = stabilize_expr(&value);
	      if (expr != NULL_TREE)
		saved_args = compound_expr(saved_args, expr);

	      // Argument needs saving.
	      if (TREE_SIDE_EFFECTS (value))
		nsaved_args++;

	      TREE_VALUE (arg) = value;
	    }
	}

      // If more than one argument needs saving, then we must enforce the
      // order of evaluation.
      if (nsaved_args > 1)
	{
	  for (tree arg = arg_list; arg != NULL_TREE; arg = TREE_CHAIN (arg))
	    {
	      tree value = TREE_VALUE (arg);

	      if (TREE_SIDE_EFFECTS (value))
		{
		  value = d_save_expr(value);
		  saved_args = compound_expr(saved_args, value);
		  TREE_VALUE (arg) = value;
		}
	    }
	}
    }

  // Evaluate the callee before calling it.
  if (saved_args != NULL_TREE && TREE_SIDE_EFFECTS (callee))
    {
      callee = d_save_expr(callee);
      saved_args = compound_expr(callee, saved_args);
    }

  tree result = d_build_call_list(TREE_TYPE (ctype), callee, arg_list);
  result = expand_intrinsic(result);

  // Return the value in a temporary slot so that it can be evaluated
  // multiple times by the caller.
  if (TREE_CODE (result) == CALL_EXPR
      && AGGREGATE_TYPE_P (TREE_TYPE (result))
      && TREE_ADDRESSABLE (TREE_TYPE (result)))
    {
      CALL_EXPR_RETURN_SLOT_OPT (result) = true;
      result = build_target_expr(result);
    }

  return compound_expr(saved_args, result);
}

// Builds a call to AssertError or AssertErrorMsg.

tree
d_assert_call (const Loc& loc, LibCall libcall, tree msg)
{
  tree args[3];
  int nargs;

  if (msg != NULL)
    {
      args[0] = msg;
      args[1] = d_array_string (loc.filename ? loc.filename : "");
      args[2] = size_int(loc.linnum);
      nargs = 3;
    }
  else
    {
      args[0] = d_array_string (loc.filename ? loc.filename : "");
      args[1] = size_int(loc.linnum);
      args[2] = NULL_TREE;
      nargs = 2;
    }

  return build_libcall (libcall, nargs, args);
}


// Our internal list of library functions.

static FuncDeclaration *libcall_decls[LIBCALL_count];

// Build and return a function symbol to be used by libcall_decls.

static FuncDeclaration *
get_libcall(const char *name, Type *type, int flags, int nparams, ...)
{
  // Add parameter types, using 'void' as the last parameter type
  // to mean this function accepts a variable list of arguments.
  Parameters *args = new Parameters;
  bool varargs = false;

  va_list ap;
  va_start (ap, nparams);
  for (int i = 0; i < nparams; i++)
    {
      Type *ptype = va_arg(ap, Type *);
      if (ptype != Type::tvoid)
	args->push(Parameter::create (0, ptype, NULL, NULL));
      else
	{
	  varargs = true;
	  break;
	}
    }
  va_end(ap);

  // Build extern(C) function.
  FuncDeclaration *decl = FuncDeclaration::genCfunc(args, type, name);

  // Set any attributes on the function, such as malloc or noreturn.
  tree t = get_symbol_decl (decl);
  set_call_expr_flags(t, flags);
  DECL_ARTIFICIAL(t) = 1;

  if (varargs)
    {
      TypeFunction *tf = (TypeFunction *) decl->type;
      tf->varargs = true;
    }

  return decl;
}

// Library functions are generated as needed.
// This could probably be changed in the future to be more like GCC builtin
// trees, but we depend on runtime initialisation of front-end types.

FuncDeclaration *
get_libcall(LibCall libcall)
{
  // Build generic AA type void*[void*] for runtime.def
  static Type *AA = NULL;
  if (AA == NULL)
    AA = TypeAArray::create (Type::tvoidptr, Type::tvoidptr);

  switch (libcall)
    {
#define DEF_D_RUNTIME(CODE, NAME, PARAMS, TYPE, FLAGS) \
    case LIBCALL_ ## CODE:	\
      libcall_decls[libcall] = get_libcall(NAME, TYPE, FLAGS, PARAMS); \
      break;
#include "runtime.def"
#undef DEF_D_RUNTIME

    default:
      gcc_unreachable();
    }

  return libcall_decls[libcall];
}

// Build call to LIBCALL. N_ARGS is the number of call arguments which are
// specified in as a tree array ARGS.  The caller can force the return type
// of the call to FORCE_TYPE if the library call returns a generic value.

// This does not perform conversions on the arguments.  This allows arbitrary data
// to be passed through varargs without going through the usual conversions.

tree
build_libcall (LibCall libcall, unsigned n_args, tree *args, tree force_type)
{
  // Build the call expression to the runtime function.
  FuncDeclaration *decl = get_libcall(libcall);
  Type *type = decl->type->nextOf();
  tree callee = build_address (get_symbol_decl (decl));
  tree arg_list = NULL_TREE;

  for (int i = n_args - 1; i >= 0; i--)
    arg_list = tree_cons (NULL_TREE, args[i], arg_list);

  tree result = d_build_call_list (build_ctype(type), callee, arg_list);

  // Assumes caller knows what it is doing.
  if (force_type != NULL_TREE)
    return convert (force_type, result);

  return result;
}

// Build a call to CALLEE, passing ARGS as arguments.  The expected return
// type is TYPE.  TREE_SIDE_EFFECTS gets set depending on the const/pure
// attributes of the funcion and the SIDE_EFFECTS flags of the arguments.

tree
d_build_call_list (tree type, tree callee, tree args)
{
  int nargs = list_length (args);
  tree *pargs = new tree[nargs];
  for (size_t i = 0; args; args = TREE_CHAIN (args), i++)
    pargs[i] = TREE_VALUE (args);

  return build_call_array (type, callee, nargs, pargs);
}

// Conveniently construct the function arguments for passing
// to the d_build_call_list function.

tree
d_build_call_nary (tree callee, int n_args, ...)
{
  va_list ap;
  tree arg_list = NULL_TREE;
  tree fntype = TREE_TYPE (callee);

  va_start (ap, n_args);
  for (int i = n_args - 1; i >= 0; i--)
    arg_list = tree_cons (NULL_TREE, va_arg (ap, tree), arg_list);
  va_end (ap);

  return d_build_call_list (TREE_TYPE (fntype), build_address (callee), nreverse (arg_list));
}

// List of codes for internally recognised compiler intrinsics.

enum intrinsic_code
{
#define DEF_D_INTRINSIC(CODE, A, N, M, D) INTRINSIC_ ## CODE,
#include "intrinsics.def"
#undef DEF_D_INTRINSIC
  INTRINSIC_LAST
};

// An internal struct used to hold information on D intrinsics.

struct intrinsic_decl
{
  // The DECL_FUNCTION_CODE of this decl.
  intrinsic_code code;

  // The name of the intrinsic.
  const char *name;

  // The module where the intrinsic is located.
  const char *module;

  // The mangled signature decoration of the intrinsic.
  const char *deco;
};

static const intrinsic_decl intrinsic_decls[] =
{
#define DEF_D_INTRINSIC(CODE, ALIAS, NAME, MODULE, DECO) \
    { INTRINSIC_ ## ALIAS, NAME, MODULE, DECO },
#include "intrinsics.def"
#undef DEF_D_INTRINSIC
};

// Call an fold the intrinsic call CALLEE with the argument ARG
// with the built-in function CODE passed.

static tree
expand_intrinsic_op (built_in_function code, tree callee, tree arg)
{
  tree exp = d_build_call_nary (builtin_decl_explicit (code), 1, arg);
  return fold_convert (TREE_TYPE (callee), fold (exp));
}

// Like expand_intrinsic_op, but takes two arguments.

static tree
expand_intrinsic_op2 (built_in_function code, tree callee, tree arg1, tree arg2)
{
  tree exp = d_build_call_nary (builtin_decl_explicit (code), 2, arg1, arg2);
  return fold_convert (TREE_TYPE (callee), fold (exp));
}

// Expand a front-end instrinsic call to bsr whose arguments are ARG.
// The original call expression is held in CALLEE.

static tree
expand_intrinsic_bsr (tree callee, tree arg)
{
  // intrinsic_code bsr gets turned into (size - 1) - count_leading_zeros(arg).
  // %% TODO: The return value is supposed to be undefined if arg is zero.
  tree type = TREE_TYPE (arg);
  tree tsize = build_integer_cst (TREE_INT_CST_LOW (TYPE_SIZE (type)) - 1, type);
  tree exp = expand_intrinsic_op (BUILT_IN_CLZL, callee, arg);

  // Handle int -> long conversions.
  if (TREE_TYPE (exp) != type)
    exp = fold_convert (type, exp);

  exp = fold_build2 (MINUS_EXPR, type, tsize, exp);
  return fold_convert (TREE_TYPE (callee), exp);
}

// Expand a front-end intrinsic call to INTRINSIC, which is either a
// call to bt, btc, btr, or bts.  These intrinsics take two arguments,
// ARG1 and ARG2, and the original call expression is held in CALLEE.

static tree
expand_intrinsic_bt (intrinsic_code intrinsic, tree callee, tree arg1, tree arg2)
{
  tree type = TREE_TYPE (TREE_TYPE (arg1));
  tree exp = build_integer_cst (TREE_INT_CST_LOW (TYPE_SIZE (type)), type);
  tree_code code;
  tree tval;

  // arg1[arg2 / exp]
  arg1 = build_array_index (arg1, fold_build2 (TRUNC_DIV_EXPR, type, arg2, exp));
  arg1 = indirect_ref (type, arg1);

  // mask = 1 << (arg2 % exp);
  arg2 = fold_build2 (TRUNC_MOD_EXPR, type, arg2, exp);
  arg2 = fold_build2 (LSHIFT_EXPR, type, size_one_node, arg2);

  // cond = arg1[arg2 / size] & mask;
  exp = fold_build2 (BIT_AND_EXPR, type, arg1, arg2);

  // cond ? -1 : 0;
  exp = build_condition(TREE_TYPE (callee), d_truthvalue_conversion (exp),
			integer_minus_one_node, integer_zero_node);

  // Update the bit as needed.
  code = (intrinsic == INTRINSIC_BTC) ? BIT_XOR_EXPR :
    (intrinsic == INTRINSIC_BTR) ? BIT_AND_EXPR :
    (intrinsic == INTRINSIC_BTS) ? BIT_IOR_EXPR : ERROR_MARK;
  gcc_assert (code != ERROR_MARK);

  // arg1[arg2 / size] op= mask
  if (intrinsic == INTRINSIC_BTR)
    arg2 = fold_build1 (BIT_NOT_EXPR, TREE_TYPE (arg2), arg2);

  tval = build_local_temp (TREE_TYPE (callee));
  exp = modify_expr (tval, exp);
  arg1 = modify_expr (arg1, fold_build2 (code, TREE_TYPE (arg1), arg1, arg2));

  return compound_expr (exp, compound_expr (arg1, tval));
}

// Expand a front-end instrinsic call to CODE, which is one of the checkedint
// intrinsics adds, addu, subs, subu, negs, muls, or mulu.
// These intrinsics take three arguments, ARG1, ARG2, and OVERFLOW, with
// exception to negs which takes two arguments, but is re-written as a call
// to subs(0, ARG2, OVERFLOW).
// The original call expression is held in CALLEE.

static tree
expand_intrinsic_arith(built_in_function code, tree callee, tree arg1,
		       tree arg2, tree overflow)
{
  tree result = build_local_temp(TREE_TYPE (callee));

  STRIP_NOPS(overflow);
  overflow = build_deref(overflow);

  // Expands to a __builtin_{add,sub,mul}_overflow.
  tree args[3];
  args[0] = arg1;
  args[1] = arg2;
  args[2] = build_address(result);

  tree fn = builtin_decl_explicit(code);
  tree exp = build_call_array(TREE_TYPE (overflow),
			      build_address(fn), 3, args);

  // Assign returned result to overflow parameter, however if
  // overflow is already true, maintain it's value.
  exp = fold_build2 (BIT_IOR_EXPR, TREE_TYPE (overflow), overflow, exp);
  overflow = modify_expr(overflow, exp);

  // Return the value of result.
  return compound_expr(overflow, result);
}

// Expand a front-end built-in call to va_arg, whose arguments are
// ARG1 and optionally ARG2.
// The original call expression is held in CALLEE.

// The cases handled here are:
//	va_arg!T(ap);
//	=>	return (T) VA_ARG_EXP<ap>
//
//	va_arg!T(ap, T arg);
//	=>	return arg = (T) VA_ARG_EXP<ap>;

static tree
expand_intrinsic_vaarg(tree callee, tree arg1, tree arg2)
{
  tree type;

  STRIP_NOPS(arg1);

  if (arg2 == NULL_TREE)
    type = TREE_TYPE(callee);
  else
    {
      STRIP_NOPS(arg2);
      gcc_assert(TREE_CODE(arg2) == ADDR_EXPR);
      arg2 = TREE_OPERAND(arg2, 0);
      type = TREE_TYPE(arg2);
    }

  tree exp = build1(VA_ARG_EXPR, type, arg1);

  if (arg2 != NULL_TREE)
    exp = modify_expr(arg2, exp);

  return exp;
}

// Expand a front-end built-in call to va_start, whose arguments are
// ARG1 and ARG2.  The original call expression is held in CALLEE.

static tree
expand_intrinsic_vastart (tree callee, tree arg1, tree arg2)
{
  // The va_list argument should already have its address taken.
  // The second argument, however, is inout and that needs to be
  // fixed to prevent a warning.

  // Could be casting... so need to check type too?
  STRIP_NOPS (arg1);
  STRIP_NOPS (arg2);
  gcc_assert (TREE_CODE (arg1) == ADDR_EXPR && TREE_CODE (arg2) == ADDR_EXPR);

  // Assuming nobody tries to change the return type.
  arg2 = TREE_OPERAND (arg2, 0);
  return expand_intrinsic_op2 (BUILT_IN_VA_START, callee, arg1, arg2);
}

// Checks if DECL is an intrinsic or runtime library function that
// requires special processing.  Marks the generated trees for DECL
// as BUILT_IN_FRONTEND so can be identified later.

void
maybe_set_intrinsic (FuncDeclaration *decl)
{
  if (!decl->ident || decl->builtin == BUILTINyes)
    return;

  // Check if it's a compiler intrinsic.  We only require that any
  // internally recognised intrinsics are declared in a module with
  // an explicit module declaration.
  Module *m = decl->getModule();
  if (!m || !m->md)
    return;

  // Look through all D intrinsics.
  TemplateInstance *ti = decl->isInstantiated();
  TemplateDeclaration *td = ti ? ti->tempdecl->isTemplateDeclaration() : NULL;
  const char *tname = decl->ident->toChars();
  const char *tmodule = m->md->toChars();
  const char *tdeco = decl->type->deco;

  for (size_t i = 0; i < (int) INTRINSIC_LAST; i++)
    {
      if (strcmp (intrinsic_decls[i].name, tname) != 0
	  || strcmp (intrinsic_decls[i].module, tmodule) != 0)
	continue;

      if (td && td->onemember)
	{
	  FuncDeclaration *fd = td->onemember->isFuncDeclaration();
	  if (fd != NULL
	      && strcmp (fd->type->toChars(), intrinsic_decls[i].deco) == 0)
	    goto Lfound;
	}
      else if (strcmp (intrinsic_decls[i].deco, tdeco) == 0)
	{
	Lfound:
	  intrinsic_code code = intrinsic_decls[i].code;

	  if (decl->csym == NULL)
	    {
	      // Store a stub BUILT_IN_FRONTEND decl.
	      decl->csym = build_decl (BUILTINS_LOCATION, FUNCTION_DECL,
				       get_identifier (tname),
				       build_ctype(decl->type));
	      DECL_LANG_SPECIFIC (decl->csym) = build_lang_decl (decl);
	      d_keep (decl->csym);
	    }

	  DECL_BUILT_IN_CLASS (decl->csym) = BUILT_IN_FRONTEND;
	  DECL_FUNCTION_CODE (decl->csym) = (built_in_function) code;
	  decl->builtin = BUILTINyes;
	  break;
	}
    }
}

//
tree
expand_volatile_load(tree arg1)
{
  gcc_assert (POINTER_TYPE_P (TREE_TYPE (arg1)));

  tree type = build_qualified_type(TREE_TYPE (arg1), TYPE_QUAL_VOLATILE);
  tree result = indirect_ref(type, arg1);
  TREE_THIS_VOLATILE (result) = 1;

  return result;
}

tree
expand_volatile_store(tree arg1, tree arg2)
{
  tree type = build_qualified_type(TREE_TYPE (arg2), TYPE_QUAL_VOLATILE);
  tree result = indirect_ref(type, arg1);
  TREE_THIS_VOLATILE (result) = 1;

  return modify_expr(result, arg2);
}

// If CALLEXP is a BUILT_IN_FRONTEND, expand and return inlined
// compiler generated instructions. Most map onto GCC builtins,
// others require a little extra work around them.

tree
expand_intrinsic(tree callexp)
{
  tree callee = CALL_EXPR_FN (callexp);

  if (TREE_CODE (callee) == ADDR_EXPR)
    callee = TREE_OPERAND (callee, 0);

  if (TREE_CODE (callee) == FUNCTION_DECL
      && DECL_BUILT_IN_CLASS (callee) == BUILT_IN_FRONTEND)
    {
      intrinsic_code intrinsic = (intrinsic_code) DECL_FUNCTION_CODE (callee);
      tree op1, op2, op3;
      tree type;

      switch (intrinsic)
	{
	case INTRINSIC_BSF:
	  // Builtin count_trailing_zeros matches behaviour of bsf
	  op1 = CALL_EXPR_ARG (callexp, 0);
	  return expand_intrinsic_op(BUILT_IN_CTZL, callexp, op1);

	case INTRINSIC_BSR:
	  op1 = CALL_EXPR_ARG (callexp, 0);
	  return expand_intrinsic_bsr(callexp, op1);

	case INTRINSIC_BTC:
	case INTRINSIC_BTR:
	case INTRINSIC_BTS:
	  op1 = CALL_EXPR_ARG (callexp, 0);
	  op2 = CALL_EXPR_ARG (callexp, 1);
	  return expand_intrinsic_bt(intrinsic, callexp, op1, op2);

	case INTRINSIC_BSWAP:
	  // Backend provides builtin bswap32.
	  // Assumes first argument and return type is uint.
	  op1 = CALL_EXPR_ARG (callexp, 0);
	  return expand_intrinsic_op(BUILT_IN_BSWAP32, callexp, op1);

	  // Math intrinsics just map to their GCC equivalents.
	case INTRINSIC_COS:
	  op1 = CALL_EXPR_ARG (callexp, 0);
	  return expand_intrinsic_op(BUILT_IN_COSL, callexp, op1);

	case INTRINSIC_SIN:
	  op1 = CALL_EXPR_ARG (callexp, 0);
	  return expand_intrinsic_op(BUILT_IN_SINL, callexp, op1);

	case INTRINSIC_RNDTOL:
	  // Not sure if llroundl stands as a good replacement for the
	  // expected behaviour of rndtol.
	  op1 = CALL_EXPR_ARG (callexp, 0);
	  return expand_intrinsic_op(BUILT_IN_LLROUNDL, callexp, op1);

	case INTRINSIC_SQRT:
	case INTRINSIC_SQRTF:
	case INTRINSIC_SQRTL:
	  // Have float, double and real variants of sqrt.
	  op1 = CALL_EXPR_ARG (callexp, 0);
	  type = TYPE_MAIN_VARIANT (TREE_TYPE (op1));
	  // op1 is an integral type - use double precision.
	  if (INTEGRAL_TYPE_P (type))
	    op1 = convert(double_type_node, op1);

	  if (intrinsic == INTRINSIC_SQRT)
	    return expand_intrinsic_op(BUILT_IN_SQRT, callexp, op1);
	  else if (intrinsic == INTRINSIC_SQRTF)
	    return expand_intrinsic_op(BUILT_IN_SQRTF, callexp, op1);
	  else if (intrinsic == INTRINSIC_SQRTL)
	    return expand_intrinsic_op(BUILT_IN_SQRTL, callexp, op1);

	  gcc_unreachable();
	  break;

	case INTRINSIC_LDEXP:
	  op1 = CALL_EXPR_ARG (callexp, 0);
	  op2 = CALL_EXPR_ARG (callexp, 1);
	  return expand_intrinsic_op2(BUILT_IN_LDEXPL, callexp, op1, op2);

	case INTRINSIC_FABS:
	  op1 = CALL_EXPR_ARG (callexp, 0);
	  return expand_intrinsic_op(BUILT_IN_FABSL, callexp, op1);

	case INTRINSIC_RINT:
	  op1 = CALL_EXPR_ARG (callexp, 0);
	  return expand_intrinsic_op(BUILT_IN_RINTL, callexp, op1);

	case INTRINSIC_VA_ARG:
	  op1 = CALL_EXPR_ARG (callexp, 0);
	  op2 = CALL_EXPR_ARG (callexp, 1);
	  return expand_intrinsic_vaarg(callexp, op1, op2);

	case INTRINSIC_C_VA_ARG:
	  op1 = CALL_EXPR_ARG (callexp, 0);
	  return expand_intrinsic_vaarg(callexp, op1, NULL_TREE);

	case INTRINSIC_VASTART:
	  op1 = CALL_EXPR_ARG (callexp, 0);
	  op2 = CALL_EXPR_ARG (callexp, 1);
	  return expand_intrinsic_vastart(callexp, op1, op2);

	case INTRINSIC_ADDS:
	case INTRINSIC_ADDSL:
	case INTRINSIC_ADDU:
	case INTRINSIC_ADDUL:
	  op1 = CALL_EXPR_ARG (callexp, 0);
	  op2 = CALL_EXPR_ARG (callexp, 1);
	  op3 = CALL_EXPR_ARG (callexp, 2);
	  return expand_intrinsic_arith(BUILT_IN_ADD_OVERFLOW,
					callexp, op1, op2, op3);

	case INTRINSIC_SUBS:
	case INTRINSIC_SUBSL:
	case INTRINSIC_SUBU:
	case INTRINSIC_SUBUL:
	  op1 = CALL_EXPR_ARG (callexp, 0);
	  op2 = CALL_EXPR_ARG (callexp, 1);
	  op3 = CALL_EXPR_ARG (callexp, 2);
	  return expand_intrinsic_arith(BUILT_IN_SUB_OVERFLOW,
					callexp, op1, op2, op3);

	case INTRINSIC_MULS:
	case INTRINSIC_MULSL:
	case INTRINSIC_MULU:
	case INTRINSIC_MULUL:
	  op1 = CALL_EXPR_ARG (callexp, 0);
	  op2 = CALL_EXPR_ARG (callexp, 1);
	  op3 = CALL_EXPR_ARG (callexp, 2);
	  return expand_intrinsic_arith(BUILT_IN_MUL_OVERFLOW,
					callexp, op1, op2, op3);

	case INTRINSIC_NEGS:
	case INTRINSIC_NEGSL:
	  op1 = fold_convert (TREE_TYPE (callexp), integer_zero_node);
	  op2 = CALL_EXPR_ARG (callexp, 0);
	  op3 = CALL_EXPR_ARG (callexp, 1);
	  return expand_intrinsic_arith(BUILT_IN_SUB_OVERFLOW,
					callexp, op1, op2, op3);

	case INTRINSIC_VLOAD:
	  op1 = CALL_EXPR_ARG (callexp, 0);
	  return expand_volatile_load(op1);

	case INTRINSIC_VSTORE:
	  op1 = CALL_EXPR_ARG (callexp, 0);
	  op2 = CALL_EXPR_ARG (callexp, 1);
	  return expand_volatile_store(op1, op2);

	default:
	  gcc_unreachable();
	}
    }

  return callexp;
}

// Build and return the correct call to fmod depending on TYPE.
// ARG0 and ARG1 are the arguments pass to the function.

tree
build_float_modulus (tree type, tree arg0, tree arg1)
{
  tree fmodfn = NULL_TREE;
  tree basetype = type;

  if (COMPLEX_FLOAT_TYPE_P (basetype))
    basetype = TREE_TYPE (basetype);

  if (TYPE_MAIN_VARIANT (basetype) == double_type_node
      || TYPE_MAIN_VARIANT (basetype) == idouble_type_node)
    fmodfn = builtin_decl_explicit (BUILT_IN_FMOD);
  else if (TYPE_MAIN_VARIANT (basetype) == float_type_node
	   || TYPE_MAIN_VARIANT (basetype) == ifloat_type_node)
    fmodfn = builtin_decl_explicit (BUILT_IN_FMODF);
  else if (TYPE_MAIN_VARIANT (basetype) == long_double_type_node
	   || TYPE_MAIN_VARIANT (basetype) == ireal_type_node)
    fmodfn = builtin_decl_explicit (BUILT_IN_FMODL);

  if (!fmodfn)
    {
      // %qT pretty prints the tree type.
      error ("tried to perform floating-point modulo division on %qT", type);
      return error_mark_node;
    }

  if (COMPLEX_FLOAT_TYPE_P (type))
    {
      tree re = d_build_call_nary(fmodfn, 2, real_part(arg0), arg1);
      tree im = d_build_call_nary(fmodfn, 2, imaginary_part(arg0), arg1);

      return complex_expr(type, re, im);
    }

  if (SCALAR_FLOAT_TYPE_P (type))
    return d_build_call_nary (fmodfn, 2, arg0, arg1);

  // Should have caught this above.
  gcc_unreachable();
}

// Build a function type whose first argument is a pointer to BASETYPE,
// which is to be used for the 'vthis' parameter for TYPE.
// The base type may be a record for member functions, or a void for
// nested functions and delegates.

tree
build_vthis_type(tree basetype, tree type)
{
  gcc_assert (TREE_CODE (type) == FUNCTION_TYPE);

  tree argtypes = tree_cons(NULL_TREE, build_pointer_type(basetype),
			    TYPE_ARG_TYPES (type));
  tree fntype = build_function_type(TREE_TYPE (type), argtypes);

  if (RECORD_OR_UNION_TYPE_P (basetype))
    TYPE_METHOD_BASETYPE (fntype) = TYPE_MAIN_VARIANT (basetype);
  else
    gcc_assert(VOID_TYPE_P (basetype));

  return fntype;
}

// If SYM is a nested function, return the static chain to be
// used when calling that function from the current function.

// If SYM is a nested class or struct, return the static chain
// to be used when creating an instance of the class from CFUN.

tree
get_frame_for_symbol (Dsymbol *sym)
{
  FuncDeclaration *func = d_function_chain ? d_function_chain->function : NULL;
  FuncDeclaration *thisfd = sym->isFuncDeclaration();
  FuncDeclaration *parentfd = NULL;

  if (thisfd != NULL)
    {
      // Check that the nested function is properly defined.
      if (!thisfd->fbody)
	{
	  // Should instead error on line that references 'thisfd'.
	  thisfd->error ("nested function missing body");
	  return null_pointer_node;
	}

      // Special case for __ensure and __require.
      if (thisfd->ident == Id::ensure || thisfd->ident == Id::require)
	parentfd = func;
      else
	parentfd = thisfd->toParent2()->isFuncDeclaration();
    }
  else
    {
      // It's a class (or struct). NewExp codegen has already determined its
      // outer scope is not another class, so it must be a function.
      while (sym && !sym->isFuncDeclaration())
	sym = sym->toParent2();

      parentfd = (FuncDeclaration *) sym;
    }

  gcc_assert (parentfd != NULL);

  if (func != parentfd)
    {
      // If no frame pointer for this function
      if (!func->vthis)
	{
	  sym->error ("is a nested function and cannot be accessed from %s", func->toChars());
	  return null_pointer_node;
	}

      // Make sure we can get the frame pointer to the outer function.
      // Go up each nesting level until we find the enclosing function.
      Dsymbol *dsym = func;

      while (thisfd != dsym)
	{
	  // Check if enclosing function is a function.
	  FuncDeclaration *fd = dsym->isFuncDeclaration();

	  if (fd != NULL)
	    {
	      if (parentfd == fd->toParent2())
		break;

	      gcc_assert (fd->isNested() || fd->vthis);
	      dsym = dsym->toParent2();
	      continue;
	    }

	  // Check if enclosed by an aggregate. That means the current
	  // function must be a member function of that aggregate.
	  AggregateDeclaration *ad = dsym->isAggregateDeclaration();

	  if (ad == NULL)
	    goto Lnoframe;
	  if (ad->isClassDeclaration() && parentfd == ad->toParent2())
	    break;
	  if (ad->isStructDeclaration() && parentfd == ad->toParent2())
	    break;

	  if (!ad->isNested() || !ad->vthis)
	    {
	    Lnoframe:
	      func->error ("cannot get frame pointer to %s",
			   sym->toPrettyChars());
	      return null_pointer_node;
	    }

	  dsym = dsym->toParent2();
	}
    }

  tree ffo = get_frameinfo (parentfd);
  if (FRAMEINFO_CREATES_FRAME (ffo) || FRAMEINFO_STATIC_CHAIN (ffo))
    return get_framedecl (func, parentfd);

  return null_pointer_node;
}

// Return the parent function of a nested class CD.

static FuncDeclaration *
d_nested_class (ClassDeclaration *cd)
{
  FuncDeclaration *fd = NULL;
  while (cd && cd->isNested())
    {
      Dsymbol *dsym = cd->toParent2();
      if ((fd = dsym->isFuncDeclaration()))
	return fd;
      else
	cd = dsym->isClassDeclaration();
    }
  return NULL;
}

// Return the parent function of a nested struct SD.

static FuncDeclaration *
d_nested_struct (StructDeclaration *sd)
{
  FuncDeclaration *fd = NULL;
  while (sd && sd->isNested())
    {
      Dsymbol *dsym = sd->toParent2();
      if ((fd = dsym->isFuncDeclaration()))
	return fd;
      else
	sd = dsym->isStructDeclaration();
    }
  return NULL;
}


// Starting from the current function FUNC, try to find a suitable value of
// 'this' in nested function instances.  A suitable 'this' value is an
// instance of OCD or a class that has OCD as a base.

static tree
find_this_tree(ClassDeclaration *ocd)
{
  FuncDeclaration *func = d_function_chain ? d_function_chain->function : NULL;

  while (func)
    {
      AggregateDeclaration *ad = func->isThis();
      ClassDeclaration *cd = ad ? ad->isClassDeclaration() : NULL;

      if (cd != NULL)
	{
	  if (ocd == cd)
	    return get_decl_tree(func->vthis);
	  else if (ocd->isBaseOf(cd, NULL))
	    return convert_expr(get_decl_tree(func->vthis), cd->type, ocd->type);

	  func = d_nested_class(cd);
	}
      else
	{
	  if (func->isNested())
	    {
	      func = func->toParent2()->isFuncDeclaration();
	      continue;
	    }

	  func = NULL;
	}
    }

  return NULL_TREE;
}

// Retrieve the outer class/struct 'this' value of DECL from
// the current function.

tree
build_vthis(AggregateDeclaration *decl)
{
  ClassDeclaration *cd = decl->isClassDeclaration();
  StructDeclaration *sd = decl->isStructDeclaration();

  // If an aggregate nested in a function has no methods and there are no
  // other nested functions, any static chain created here will never be
  // translated.  Use a null pointer for the link in this case.
  tree vthis_value = null_pointer_node;

  if (cd != NULL || sd != NULL)
    {
      Dsymbol *outer = decl->toParent2();

      // If the parent is a templated struct, the outer context is instead
      // the enclosing symbol of where the instantiation happened.
      if (outer->isStructDeclaration())
	{
	  gcc_assert(outer->parent && outer->parent->isTemplateInstance());
	  outer = ((TemplateInstance *) outer->parent)->enclosing;
	}

      // For outer classes, get a suitable 'this' value.
      // For outer functions, get a suitable frame/closure pointer.
      ClassDeclaration *cdo = outer->isClassDeclaration();
      FuncDeclaration *fdo = outer->isFuncDeclaration();

      if (cdo)
	{
	  vthis_value = find_this_tree(cdo);
	  gcc_assert(vthis_value != NULL_TREE);
	}
      else if (fdo)
	{
	  tree ffo = get_frameinfo(fdo);
	  if (FRAMEINFO_CREATES_FRAME (ffo) || FRAMEINFO_STATIC_CHAIN (ffo)
	      || fdo->hasNestedFrameRefs())
	    vthis_value = get_frame_for_symbol(decl);
	  else if (cd != NULL)
	    {
	      // Classes nested in methods are allowed to access any outer
	      // class fields, use the function chain in this case.
	      if (fdo->vthis && fdo->vthis->type != Type::tvoidptr)
		vthis_value = get_decl_tree(fdo->vthis);
	    }
	}
      else
	gcc_unreachable();
    }

  return vthis_value;
}

static tree
build_frame_type (tree ffi, FuncDeclaration *fd)
{
  if (FRAMEINFO_TYPE (ffi))
    return FRAMEINFO_TYPE (ffi);

  tree frame_rec_type = make_node (RECORD_TYPE);
  char *name = concat (FRAMEINFO_IS_CLOSURE (ffi) ? "CLOSURE." : "FRAME.",
		       fd->toPrettyChars(), NULL);
  TYPE_NAME (frame_rec_type) = get_identifier (name);
  free (name);

  tree ptr_field = build_decl (BUILTINS_LOCATION, FIELD_DECL,
			       get_identifier ("__chain"), ptr_type_node);
  DECL_FIELD_CONTEXT (ptr_field) = frame_rec_type;
  TYPE_READONLY (frame_rec_type) = 1;

  tree fields = chainon (NULL_TREE, ptr_field);

  /* The __ensure and __require are called directly, so never make the outer
     functions closure, but nevertheless could still be referencing parameters
     of the calling function non-locally.  So we add all parameters with nested
     refs to the function frame, this should also mean overriding methods will
     have the same frame layout when inheriting a contract.  */
  if ((global.params.useIn && fd->frequire)
      || (global.params.useOut && fd->fensure))
    {
      if (fd->parameters)
	{
	  for (size_t i = 0; fd->parameters && i < fd->parameters->dim; i++)
	    {
	      VarDeclaration *v = (*fd->parameters)[i];
	      // Remove if already in closureVars so can push to front.
	      for (size_t j = i; j < fd->closureVars.dim; j++)
		{
		  Dsymbol *s = fd->closureVars[j];
		  if (s == v)
		    {
		      fd->closureVars.remove (j);
		      break;
		    }
		}
	      fd->closureVars.insert (i, v);
	    }
	}

      // Also add hidden 'this' to outer context.
      if (fd->vthis)
	{
	  for (size_t i = 0; i < fd->closureVars.dim; i++)
	    {
	      Dsymbol *s = fd->closureVars[i];
	      if (s == fd->vthis)
		{
		  fd->closureVars.remove (i);
		  break;
		}
	    }
	  fd->closureVars.insert (0, fd->vthis);
	}
    }

  for (size_t i = 0; i < fd->closureVars.dim; i++)
    {
      VarDeclaration *v = fd->closureVars[i];
      tree s = get_symbol_decl (v);
      tree field = build_decl (get_linemap (v->loc), FIELD_DECL,
			       v->ident ? get_identifier (v->ident->toChars()) : NULL_TREE,
			       declaration_type (v));
      SET_DECL_LANG_FRAME_FIELD (s, field);
      DECL_FIELD_CONTEXT (field) = frame_rec_type;
      fields = chainon (fields, field);
      TREE_USED (s) = 1;

      // Can't do nrvo if the variable is put in a frame.
      if (fd->nrvo_can && fd->nrvo_var == v)
	fd->nrvo_can = 0;

      if (FRAMEINFO_IS_CLOSURE (ffi))
	{
	  // Because the value needs to survive the end of the scope.
	  if ((v->edtor && (v->storage_class & STCparameter))
	      || v->needsScopeDtor())
	    v->error("has scoped destruction, cannot build closure");
	}
    }

  TYPE_FIELDS (frame_rec_type) = fields;
  layout_type (frame_rec_type);
  d_keep (frame_rec_type);

  return frame_rec_type;
}

// Closures are implemented by taking the local variables that
// need to survive the scope of the function, and copying them
// into a gc allocated chuck of memory. That chunk, called the
// closure here, is inserted into the linked list of stack
// frames instead of the usual stack frame.

// If a closure is not required, but FD still needs a frame to lower
// nested refs, then instead build custom static chain decl on stack.

void
build_closure(FuncDeclaration *fd)
{
  tree ffi = get_frameinfo (fd);

  if (!FRAMEINFO_CREATES_FRAME (ffi))
    return;

  tree type = FRAMEINFO_TYPE (ffi);
  gcc_assert(COMPLETE_TYPE_P(type));

  tree decl, decl_ref;

  if (FRAMEINFO_IS_CLOSURE (ffi))
    {
      decl = build_local_temp(build_pointer_type(type));
      DECL_NAME(decl) = get_identifier("__closptr");
      decl_ref = build_deref(decl);

      // Allocate memory for closure.
      tree arg = convert(build_ctype(Type::tsize_t), TYPE_SIZE_UNIT(type));
      tree init = build_libcall(LIBCALL_ALLOCMEMORY, 1, &arg);

      tree init_exp = build_assign (INIT_EXPR, decl,
				    build_nop (TREE_TYPE (decl), init));
      add_stmt (init_exp);
    }
  else
    {
      decl = build_local_temp(type);
      DECL_NAME(decl) = get_identifier("__frame");
      decl_ref = decl;
    }

  DECL_IGNORED_P(decl) = 0;

  // Set the first entry to the parent closure/frame, if any.
  tree chain_field = component_ref(decl_ref, TYPE_FIELDS(type));
  tree chain_expr = modify_expr(chain_field, d_function_chain->static_chain);
  add_stmt(chain_expr);

  // Copy parameters that are referenced nonlocally.
  for (size_t i = 0; i < fd->closureVars.dim; i++)
    {
      VarDeclaration *v = fd->closureVars[i];

      if (!v->isParameter())
	continue;

      tree vsym = get_symbol_decl (v);

      tree field = component_ref (decl_ref, DECL_LANG_FRAME_FIELD (vsym));
      tree expr = modify_expr (field, vsym);
      add_stmt(expr);
    }

  if (!FRAMEINFO_IS_CLOSURE (ffi))
    decl = build_address (decl);

  d_function_chain->static_chain = decl;
}

// Return the frame of FD.  This could be a static chain or a closure
// passed via the hidden 'this' pointer.

tree
get_frameinfo(FuncDeclaration *fd)
{
  tree fds = get_symbol_decl (fd);
  if (DECL_LANG_FRAMEINFO (fds))
    return DECL_LANG_FRAMEINFO (fds);

  tree ffi = make_node (FUNCFRAME_INFO);

  DECL_LANG_FRAMEINFO (fds) = ffi;

  // Nested functions, or functions with nested refs must create
  // a static frame for local variables to be referenced from.
  if (fd->hasNestedFrameRefs()
      || (fd->vthis && fd->vthis->type == Type::tvoidptr))
    FRAMEINFO_CREATES_FRAME (ffi) = 1;

  /* In checkNestedReference, references from contracts are not added to the
     closureVars array, so assume all parameters referenced.  Even if they
     aren't the 'this' parameter may still be needed for the static chain.  */
  if ((global.params.useIn && fd->frequire)
      || (global.params.useOut && fd->fensure))
    FRAMEINFO_CREATES_FRAME (ffi) = 1;

  // D2 maybe setup closure instead.
  if (fd->needsClosure())
    {
      FRAMEINFO_CREATES_FRAME (ffi) = 1;
      FRAMEINFO_IS_CLOSURE (ffi) = 1;
    }
  else if (fd->closureVars.dim == 0)
    {
      /* If fd is nested (deeply) in a function that creates a closure,
	 then fd inherits that closure via hidden vthis pointer, and
	 doesn't create a stack frame at all.  */
      FuncDeclaration *ff = fd;

      while (ff)
	{
	  tree ffo = get_frameinfo (ff);

	  if (ff != fd && FRAMEINFO_CREATES_FRAME (ffo))
	    {
	      gcc_assert (FRAMEINFO_TYPE (ffo));
	      FRAMEINFO_CREATES_FRAME (ffi) = 0;
	      FRAMEINFO_STATIC_CHAIN (ffi) = 1;
	      FRAMEINFO_IS_CLOSURE (ffi) = FRAMEINFO_IS_CLOSURE (ffo);
	      gcc_assert (COMPLETE_TYPE_P (FRAMEINFO_TYPE (ffo)));
	      FRAMEINFO_TYPE (ffi) = FRAMEINFO_TYPE (ffo);
	      break;
	    }

	  // Stop looking if no frame pointer for this function.
	  if (ff->vthis == NULL)
	    break;

	  AggregateDeclaration *ad = ff->isThis();
	  if (ad && ad->isNested())
	    {
	      while (ad->isNested())
		{
		  Dsymbol *d = ad->toParent2();
		  ad = d->isAggregateDeclaration();
		  ff = d->isFuncDeclaration();

		  if (ad == NULL)
		    break;
		}
	    }
	  else
	    ff = ff->toParent2()->isFuncDeclaration();
	}
    }

  // Build type now as may be referenced from another module.
  if (FRAMEINFO_CREATES_FRAME (ffi))
    FRAMEINFO_TYPE (ffi) = build_frame_type (ffi, fd);

  return ffi;
}

// Return a pointer to the frame/closure block of OUTER
// so can be accessed from the function INNER.

tree
get_framedecl (FuncDeclaration *inner, FuncDeclaration *outer)
{
  tree result = d_function_chain->static_chain;
  FuncDeclaration *fd = inner;

  while (fd && fd != outer)
    {
      AggregateDeclaration *ad;
      ClassDeclaration *cd;
      StructDeclaration *sd;

      // Parent frame link is the first field.
      if (FRAMEINFO_CREATES_FRAME (get_frameinfo (fd)))
	result = indirect_ref (ptr_type_node, result);

      if (fd->isNested())
	fd = fd->toParent2()->isFuncDeclaration();
      // The frame/closure record always points to the outer function's
      // frame, even if there are intervening nested classes or structs.
      // So, we can just skip over these...
      else if ((ad = fd->isThis()) && (cd = ad->isClassDeclaration()))
	fd = d_nested_class (cd);
      else if ((ad = fd->isThis()) && (sd = ad->isStructDeclaration()))
	fd = d_nested_struct (sd);
      else
	break;
    }

  // Go get our frame record.
  gcc_assert (fd == outer);
  tree frame_type = FRAMEINFO_TYPE (get_frameinfo (outer));

  if (frame_type != NULL_TREE)
    {
      result = build_nop (build_pointer_type (frame_type), result);
      return result;
    }
  else
    {
      inner->error ("forward reference to frame of %s", outer->toChars());
      return null_pointer_node;
    }
}


// Create a declaration for field NAME of a given TYPE, setting the flags
// for whether the field is ARTIFICIAL and/or IGNORED.

tree
create_field_decl (tree type, const char *name, int artificial, int ignored)
{
  tree decl = build_decl (input_location, FIELD_DECL,
			  name ? get_identifier (name) : NULL_TREE, type);
  DECL_ARTIFICIAL (decl) = artificial;
  DECL_IGNORED_P (decl) = ignored;

  return decl;
}

// Find the field inside aggregate TYPE identified by IDENT at field OFFSET.

tree
find_aggregate_field(tree type, tree ident, tree offset)
{
  tree fields = TYPE_FIELDS (type);

  for (tree field = fields; field != NULL_TREE; field = TREE_CHAIN (field))
    {
      if (DECL_NAME (field) == NULL_TREE
	  && RECORD_OR_UNION_TYPE_P (TREE_TYPE (field))
	  && ANON_AGGR_TYPE_P (TREE_TYPE (field)))
	{
	  // Search nesting anonymous structs and unions.
	  tree vfield = find_aggregate_field(TREE_TYPE (field),
					     ident, offset);
	  if (vfield != NULL_TREE)
	    return vfield;
	}
      else if (DECL_NAME (field) == ident
	       && (offset == NULL_TREE
		   || DECL_FIELD_OFFSET (field) == offset))
	{
	  // Found matching field at offset.
	  return field;
	}
    }

  return NULL_TREE;
}
