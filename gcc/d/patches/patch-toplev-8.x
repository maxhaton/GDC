This implements building of libphobos library in GCC.
---

--- a/Makefile.def
+++ b/Makefile.def
@@ -158,6 +158,7 @@ target_modules = { module= libgfortran; };
 target_modules = { module= libobjc; };
 target_modules = { module= libgo; };
 target_modules = { module= libhsail-rt; };
+target_modules = { module= libphobos; };
 target_modules = { module= libtermcap; no_check=true;
                    missing=mostlyclean;
                    missing=clean;
@@ -283,6 +284,7 @@ flags_to_pass = { flag= FLAGS_FOR_TARGET ; };
 flags_to_pass = { flag= GFORTRAN_FOR_TARGET ; };
 flags_to_pass = { flag= GOC_FOR_TARGET ; };
 flags_to_pass = { flag= GOCFLAGS_FOR_TARGET ; };
+flags_to_pass = { flag= GDC_FOR_TARGET ; };
 flags_to_pass = { flag= LD_FOR_TARGET ; };
 flags_to_pass = { flag= LIPO_FOR_TARGET ; };
 flags_to_pass = { flag= LDFLAGS_FOR_TARGET ; };
@@ -550,6 +552,8 @@ dependencies = { module=configure-target-libgo; on=all-target-libstdc++-v3; };
 dependencies = { module=all-target-libgo; on=all-target-libbacktrace; };
 dependencies = { module=all-target-libgo; on=all-target-libffi; };
 dependencies = { module=all-target-libgo; on=all-target-libatomic; };
+dependencies = { module=configure-target-libphobos; on=configure-target-zlib; };
+dependencies = { module=all-target-libphobos; on=all-target-zlib; };
 dependencies = { module=configure-target-libstdc++-v3; on=configure-target-libgomp; };
 dependencies = { module=configure-target-liboffloadmic; on=configure-target-libgomp; };
 dependencies = { module=configure-target-libsanitizer; on=all-target-libstdc++-v3; };
@@ -604,6 +608,8 @@ languages = { language=go;	gcc-check-target=check-go;
 				lib-check-target=check-target-libgo; };
 languages = { language=brig;	gcc-check-target=check-brig;
 				lib-check-target=check-target-libhsail-rt; };
+languages = { language=d;	gcc-check-target=check-d;
+				lib-check-target=check-target-libphobos; };
 
 // Toplevel bootstrap
 bootstrap_stage = { id=1 ; };
--- a/Makefile.in
+++ b/Makefile.in
@@ -156,6 +156,7 @@ BUILD_EXPORTS = \
 	GFORTRAN="$(GFORTRAN_FOR_BUILD)"; export GFORTRAN; \
 	GOC="$(GOC_FOR_BUILD)"; export GOC; \
 	GOCFLAGS="$(GOCFLAGS_FOR_BUILD)"; export GOCFLAGS; \
+	GDC="$(GDC_FOR_BUILD)"; export GDC; \
 	DLLTOOL="$(DLLTOOL_FOR_BUILD)"; export DLLTOOL; \
 	LD="$(LD_FOR_BUILD)"; export LD; \
 	LDFLAGS="$(LDFLAGS_FOR_BUILD)"; export LDFLAGS; \
@@ -192,6 +193,7 @@ HOST_EXPORTS = \
 	CXXFLAGS="$(CXXFLAGS)"; export CXXFLAGS; \
 	GFORTRAN="$(GFORTRAN)"; export GFORTRAN; \
 	GOC="$(GOC)"; export GOC; \
+	GDC="$(GDC)"; export GDC; \
 	AR="$(AR)"; export AR; \
 	AS="$(AS)"; export AS; \
 	CC_FOR_BUILD="$(CC_FOR_BUILD)"; export CC_FOR_BUILD; \
@@ -278,6 +280,7 @@ BASE_TARGET_EXPORTS = \
 	CXXFLAGS="$(CXXFLAGS_FOR_TARGET)"; export CXXFLAGS; \
 	GFORTRAN="$(GFORTRAN_FOR_TARGET) $(XGCC_FLAGS_FOR_TARGET) $$TFLAGS"; export GFORTRAN; \
 	GOC="$(GOC_FOR_TARGET) $(XGCC_FLAGS_FOR_TARGET) $$TFLAGS"; export GOC; \
+	GDC="$(GDC_FOR_TARGET) $(XGCC_FLAGS_FOR_TARGET) $$TFLAGS"; export GDC; \
 	DLLTOOL="$(DLLTOOL_FOR_TARGET)"; export DLLTOOL; \
 	LD="$(COMPILER_LD_FOR_TARGET)"; export LD; \
 	LDFLAGS="$(LDFLAGS_FOR_TARGET)"; export LDFLAGS; \
@@ -342,6 +345,7 @@ CXX_FOR_BUILD = @CXX_FOR_BUILD@
 DLLTOOL_FOR_BUILD = @DLLTOOL_FOR_BUILD@
 GFORTRAN_FOR_BUILD = @GFORTRAN_FOR_BUILD@
 GOC_FOR_BUILD = @GOC_FOR_BUILD@
+GDC_FOR_BUILD = @GDC_FOR_BUILD@
 LDFLAGS_FOR_BUILD = @LDFLAGS_FOR_BUILD@
 LD_FOR_BUILD = @LD_FOR_BUILD@
 NM_FOR_BUILD = @NM_FOR_BUILD@
@@ -551,6 +555,7 @@ CXX_FOR_TARGET=$(STAGE_CC_WRAPPER) @CXX_FOR_TARGET@
 RAW_CXX_FOR_TARGET=$(STAGE_CC_WRAPPER) @RAW_CXX_FOR_TARGET@
 GFORTRAN_FOR_TARGET=$(STAGE_CC_WRAPPER) @GFORTRAN_FOR_TARGET@
 GOC_FOR_TARGET=$(STAGE_CC_WRAPPER) @GOC_FOR_TARGET@
+GDC_FOR_TARGET=$(STAGE_CC_WRAPPER) @GDC_FOR_TARGET@
 DLLTOOL_FOR_TARGET=@DLLTOOL_FOR_TARGET@
 LD_FOR_TARGET=@LD_FOR_TARGET@
 
@@ -774,6 +779,7 @@ BASE_FLAGS_TO_PASS = \
 	"GFORTRAN_FOR_TARGET=$(GFORTRAN_FOR_TARGET)" \
 	"GOC_FOR_TARGET=$(GOC_FOR_TARGET)" \
 	"GOCFLAGS_FOR_TARGET=$(GOCFLAGS_FOR_TARGET)" \
+	"GDC_FOR_TARGET=$(GDC_FOR_TARGET)" \
 	"LD_FOR_TARGET=$(LD_FOR_TARGET)" \
 	"LIPO_FOR_TARGET=$(LIPO_FOR_TARGET)" \
 	"LDFLAGS_FOR_TARGET=$(LDFLAGS_FOR_TARGET)" \
@@ -833,6 +839,7 @@ EXTRA_HOST_FLAGS = \
 	'DLLTOOL=$(DLLTOOL)' \
 	'GFORTRAN=$(GFORTRAN)' \
 	'GOC=$(GOC)' \
+	'GDC=$(GDC)' \
 	'LD=$(LD)' \
 	'LIPO=$(LIPO)' \
 	'NM=$(NM)' \
@@ -889,6 +896,7 @@ EXTRA_TARGET_FLAGS = \
 	'GFORTRAN=$$(GFORTRAN_FOR_TARGET) $$(XGCC_FLAGS_FOR_TARGET) $$(TFLAGS)' \
 	'GOC=$$(GOC_FOR_TARGET) $$(XGCC_FLAGS_FOR_TARGET) $$(TFLAGS)' \
 	'GOCFLAGS=$$(GOCFLAGS_FOR_TARGET)' \
+	'GDC=$$(GDC_FOR_TARGET) $$(XGCC_FLAGS_FOR_TARGET) $$(TFLAGS)' \
 	'LD=$(COMPILER_LD_FOR_TARGET)' \
 	'LDFLAGS=$$(LDFLAGS_FOR_TARGET)' \
 	'LIBCFLAGS=$$(LIBCFLAGS_FOR_TARGET)' \
@@ -992,6 +1000,7 @@ configure-target:  \
     maybe-configure-target-libobjc \
     maybe-configure-target-libgo \
     maybe-configure-target-libhsail-rt \
+    maybe-configure-target-libphobos \
     maybe-configure-target-libtermcap \
     maybe-configure-target-winsup \
     maybe-configure-target-libgloss \
@@ -1158,6 +1167,7 @@ all-target: maybe-all-target-libgfortran
 all-target: maybe-all-target-libobjc
 all-target: maybe-all-target-libgo
 all-target: maybe-all-target-libhsail-rt
+all-target: maybe-all-target-libphobos
 all-target: maybe-all-target-libtermcap
 all-target: maybe-all-target-winsup
 all-target: maybe-all-target-libgloss
@@ -1251,6 +1261,7 @@ info-target: maybe-info-target-libgfortran
 info-target: maybe-info-target-libobjc
 info-target: maybe-info-target-libgo
 info-target: maybe-info-target-libhsail-rt
+info-target: maybe-info-target-libphobos
 info-target: maybe-info-target-libtermcap
 info-target: maybe-info-target-winsup
 info-target: maybe-info-target-libgloss
@@ -1337,6 +1348,7 @@ dvi-target: maybe-dvi-target-libgfortran
 dvi-target: maybe-dvi-target-libobjc
 dvi-target: maybe-dvi-target-libgo
 dvi-target: maybe-dvi-target-libhsail-rt
+dvi-target: maybe-dvi-target-libphobos
 dvi-target: maybe-dvi-target-libtermcap
 dvi-target: maybe-dvi-target-winsup
 dvi-target: maybe-dvi-target-libgloss
@@ -1423,6 +1435,7 @@ pdf-target: maybe-pdf-target-libgfortran
 pdf-target: maybe-pdf-target-libobjc
 pdf-target: maybe-pdf-target-libgo
 pdf-target: maybe-pdf-target-libhsail-rt
+pdf-target: maybe-pdf-target-libphobos
 pdf-target: maybe-pdf-target-libtermcap
 pdf-target: maybe-pdf-target-winsup
 pdf-target: maybe-pdf-target-libgloss
@@ -1509,6 +1522,7 @@ html-target: maybe-html-target-libgfortran
 html-target: maybe-html-target-libobjc
 html-target: maybe-html-target-libgo
 html-target: maybe-html-target-libhsail-rt
+html-target: maybe-html-target-libphobos
 html-target: maybe-html-target-libtermcap
 html-target: maybe-html-target-winsup
 html-target: maybe-html-target-libgloss
@@ -1595,6 +1609,7 @@ TAGS-target: maybe-TAGS-target-libgfortran
 TAGS-target: maybe-TAGS-target-libobjc
 TAGS-target: maybe-TAGS-target-libgo
 TAGS-target: maybe-TAGS-target-libhsail-rt
+TAGS-target: maybe-TAGS-target-libphobos
 TAGS-target: maybe-TAGS-target-libtermcap
 TAGS-target: maybe-TAGS-target-winsup
 TAGS-target: maybe-TAGS-target-libgloss
@@ -1681,6 +1696,7 @@ install-info-target: maybe-install-info-target-libgfortran
 install-info-target: maybe-install-info-target-libobjc
 install-info-target: maybe-install-info-target-libgo
 install-info-target: maybe-install-info-target-libhsail-rt
+install-info-target: maybe-install-info-target-libphobos
 install-info-target: maybe-install-info-target-libtermcap
 install-info-target: maybe-install-info-target-winsup
 install-info-target: maybe-install-info-target-libgloss
@@ -1767,6 +1783,7 @@ install-pdf-target: maybe-install-pdf-target-libgfortran
 install-pdf-target: maybe-install-pdf-target-libobjc
 install-pdf-target: maybe-install-pdf-target-libgo
 install-pdf-target: maybe-install-pdf-target-libhsail-rt
+install-pdf-target: maybe-install-pdf-target-libphobos
 install-pdf-target: maybe-install-pdf-target-libtermcap
 install-pdf-target: maybe-install-pdf-target-winsup
 install-pdf-target: maybe-install-pdf-target-libgloss
@@ -1853,6 +1870,7 @@ install-html-target: maybe-install-html-target-libgfortran
 install-html-target: maybe-install-html-target-libobjc
 install-html-target: maybe-install-html-target-libgo
 install-html-target: maybe-install-html-target-libhsail-rt
+install-html-target: maybe-install-html-target-libphobos
 install-html-target: maybe-install-html-target-libtermcap
 install-html-target: maybe-install-html-target-winsup
 install-html-target: maybe-install-html-target-libgloss
@@ -1939,6 +1957,7 @@ installcheck-target: maybe-installcheck-target-libgfortran
 installcheck-target: maybe-installcheck-target-libobjc
 installcheck-target: maybe-installcheck-target-libgo
 installcheck-target: maybe-installcheck-target-libhsail-rt
+installcheck-target: maybe-installcheck-target-libphobos
 installcheck-target: maybe-installcheck-target-libtermcap
 installcheck-target: maybe-installcheck-target-winsup
 installcheck-target: maybe-installcheck-target-libgloss
@@ -2025,6 +2044,7 @@ mostlyclean-target: maybe-mostlyclean-target-libgfortran
 mostlyclean-target: maybe-mostlyclean-target-libobjc
 mostlyclean-target: maybe-mostlyclean-target-libgo
 mostlyclean-target: maybe-mostlyclean-target-libhsail-rt
+mostlyclean-target: maybe-mostlyclean-target-libphobos
 mostlyclean-target: maybe-mostlyclean-target-libtermcap
 mostlyclean-target: maybe-mostlyclean-target-winsup
 mostlyclean-target: maybe-mostlyclean-target-libgloss
@@ -2111,6 +2131,7 @@ clean-target: maybe-clean-target-libgfortran
 clean-target: maybe-clean-target-libobjc
 clean-target: maybe-clean-target-libgo
 clean-target: maybe-clean-target-libhsail-rt
+clean-target: maybe-clean-target-libphobos
 clean-target: maybe-clean-target-libtermcap
 clean-target: maybe-clean-target-winsup
 clean-target: maybe-clean-target-libgloss
@@ -2197,6 +2218,7 @@ distclean-target: maybe-distclean-target-libgfortran
 distclean-target: maybe-distclean-target-libobjc
 distclean-target: maybe-distclean-target-libgo
 distclean-target: maybe-distclean-target-libhsail-rt
+distclean-target: maybe-distclean-target-libphobos
 distclean-target: maybe-distclean-target-libtermcap
 distclean-target: maybe-distclean-target-winsup
 distclean-target: maybe-distclean-target-libgloss
@@ -2283,6 +2305,7 @@ maintainer-clean-target: maybe-maintainer-clean-target-libgfortran
 maintainer-clean-target: maybe-maintainer-clean-target-libobjc
 maintainer-clean-target: maybe-maintainer-clean-target-libgo
 maintainer-clean-target: maybe-maintainer-clean-target-libhsail-rt
+maintainer-clean-target: maybe-maintainer-clean-target-libphobos
 maintainer-clean-target: maybe-maintainer-clean-target-libtermcap
 maintainer-clean-target: maybe-maintainer-clean-target-winsup
 maintainer-clean-target: maybe-maintainer-clean-target-libgloss
@@ -2425,6 +2448,7 @@ check-target:  \
     maybe-check-target-libobjc \
     maybe-check-target-libgo \
     maybe-check-target-libhsail-rt \
+    maybe-check-target-libphobos \
     maybe-check-target-libtermcap \
     maybe-check-target-winsup \
     maybe-check-target-libgloss \
@@ -2607,6 +2631,7 @@ install-target:  \
     maybe-install-target-libobjc \
     maybe-install-target-libgo \
     maybe-install-target-libhsail-rt \
+    maybe-install-target-libphobos \
     maybe-install-target-libtermcap \
     maybe-install-target-winsup \
     maybe-install-target-libgloss \
@@ -2713,6 +2738,7 @@ install-strip-target:  \
     maybe-install-strip-target-libobjc \
     maybe-install-strip-target-libgo \
     maybe-install-strip-target-libhsail-rt \
+    maybe-install-strip-target-libphobos \
     maybe-install-strip-target-libtermcap \
     maybe-install-strip-target-winsup \
     maybe-install-strip-target-libgloss \
@@ -46570,6 +46596,464 @@ maintainer-clean-target-libhsail-rt:
 
 
 
+.PHONY: configure-target-libphobos maybe-configure-target-libphobos
+maybe-configure-target-libphobos:
+@if gcc-bootstrap
+configure-target-libphobos: stage_current
+@endif gcc-bootstrap
+@if target-libphobos
+maybe-configure-target-libphobos: configure-target-libphobos
+configure-target-libphobos: 
+	@: $(MAKE); $(unstage)
+	@r=`${PWD_COMMAND}`; export r; \
+	s=`cd $(srcdir); ${PWD_COMMAND}`; export s; \
+	echo "Checking multilib configuration for libphobos..."; \
+	$(SHELL) $(srcdir)/mkinstalldirs $(TARGET_SUBDIR)/libphobos; \
+	$(CC_FOR_TARGET) --print-multi-lib > $(TARGET_SUBDIR)/libphobos/multilib.tmp 2> /dev/null; \
+	if test -r $(TARGET_SUBDIR)/libphobos/multilib.out; then \
+	  if cmp -s $(TARGET_SUBDIR)/libphobos/multilib.tmp $(TARGET_SUBDIR)/libphobos/multilib.out; then \
+	    rm -f $(TARGET_SUBDIR)/libphobos/multilib.tmp; \
+	  else \
+	    rm -f $(TARGET_SUBDIR)/libphobos/Makefile; \
+	    mv $(TARGET_SUBDIR)/libphobos/multilib.tmp $(TARGET_SUBDIR)/libphobos/multilib.out; \
+	  fi; \
+	else \
+	  mv $(TARGET_SUBDIR)/libphobos/multilib.tmp $(TARGET_SUBDIR)/libphobos/multilib.out; \
+	fi; \
+	test ! -f $(TARGET_SUBDIR)/libphobos/Makefile || exit 0; \
+	$(SHELL) $(srcdir)/mkinstalldirs $(TARGET_SUBDIR)/libphobos; \
+	$(NORMAL_TARGET_EXPORTS)  \
+	echo Configuring in $(TARGET_SUBDIR)/libphobos; \
+	cd "$(TARGET_SUBDIR)/libphobos" || exit 1; \
+	case $(srcdir) in \
+	  /* | [A-Za-z]:[\\/]*) topdir=$(srcdir) ;; \
+	  *) topdir=`echo $(TARGET_SUBDIR)/libphobos/ | \
+		sed -e 's,\./,,g' -e 's,[^/]*/,../,g' `$(srcdir) ;; \
+	esac; \
+	module_srcdir=libphobos; \
+	rm -f no-such-file || : ; \
+	CONFIG_SITE=no-such-file $(SHELL) \
+	  $$s/$$module_srcdir/configure \
+	  --srcdir=$${topdir}/$$module_srcdir \
+	  $(TARGET_CONFIGARGS) --build=${build_alias} --host=${target_alias} \
+	  --target=${target_alias}  \
+	  || exit 1
+@endif target-libphobos
+
+
+
+
+
+.PHONY: all-target-libphobos maybe-all-target-libphobos
+maybe-all-target-libphobos:
+@if gcc-bootstrap
+all-target-libphobos: stage_current
+@endif gcc-bootstrap
+@if target-libphobos
+TARGET-target-libphobos=all
+maybe-all-target-libphobos: all-target-libphobos
+all-target-libphobos: configure-target-libphobos
+	@: $(MAKE); $(unstage)
+	@r=`${PWD_COMMAND}`; export r; \
+	s=`cd $(srcdir); ${PWD_COMMAND}`; export s; \
+	$(NORMAL_TARGET_EXPORTS)  \
+	(cd $(TARGET_SUBDIR)/libphobos && \
+	  $(MAKE) $(BASE_FLAGS_TO_PASS) $(EXTRA_TARGET_FLAGS)   \
+		$(TARGET-target-libphobos))
+@endif target-libphobos
+
+
+
+
+
+.PHONY: check-target-libphobos maybe-check-target-libphobos
+maybe-check-target-libphobos:
+@if target-libphobos
+maybe-check-target-libphobos: check-target-libphobos
+
+check-target-libphobos:
+	@: $(MAKE); $(unstage)
+	@r=`${PWD_COMMAND}`; export r; \
+	s=`cd $(srcdir); ${PWD_COMMAND}`; export s; \
+	$(NORMAL_TARGET_EXPORTS) \
+	(cd $(TARGET_SUBDIR)/libphobos && \
+	  $(MAKE) $(TARGET_FLAGS_TO_PASS)   check)
+
+@endif target-libphobos
+
+.PHONY: install-target-libphobos maybe-install-target-libphobos
+maybe-install-target-libphobos:
+@if target-libphobos
+maybe-install-target-libphobos: install-target-libphobos
+
+install-target-libphobos: installdirs
+	@: $(MAKE); $(unstage)
+	@r=`${PWD_COMMAND}`; export r; \
+	s=`cd $(srcdir); ${PWD_COMMAND}`; export s; \
+	$(NORMAL_TARGET_EXPORTS) \
+	(cd $(TARGET_SUBDIR)/libphobos && \
+	  $(MAKE) $(TARGET_FLAGS_TO_PASS)  install)
+
+@endif target-libphobos
+
+.PHONY: install-strip-target-libphobos maybe-install-strip-target-libphobos
+maybe-install-strip-target-libphobos:
+@if target-libphobos
+maybe-install-strip-target-libphobos: install-strip-target-libphobos
+
+install-strip-target-libphobos: installdirs
+	@: $(MAKE); $(unstage)
+	@r=`${PWD_COMMAND}`; export r; \
+	s=`cd $(srcdir); ${PWD_COMMAND}`; export s; \
+	$(NORMAL_TARGET_EXPORTS) \
+	(cd $(TARGET_SUBDIR)/libphobos && \
+	  $(MAKE) $(TARGET_FLAGS_TO_PASS)  install-strip)
+
+@endif target-libphobos
+
+# Other targets (info, dvi, pdf, etc.)
+
+.PHONY: maybe-info-target-libphobos info-target-libphobos
+maybe-info-target-libphobos:
+@if target-libphobos
+maybe-info-target-libphobos: info-target-libphobos
+
+info-target-libphobos: \
+    configure-target-libphobos 
+	@: $(MAKE); $(unstage)
+	@[ -f $(TARGET_SUBDIR)/libphobos/Makefile ] || exit 0; \
+	r=`${PWD_COMMAND}`; export r; \
+	s=`cd $(srcdir); ${PWD_COMMAND}`; export s; \
+	$(NORMAL_TARGET_EXPORTS) \
+	echo "Doing info in $(TARGET_SUBDIR)/libphobos"; \
+	for flag in $(EXTRA_TARGET_FLAGS); do \
+	  eval `echo "$$flag" | sed -e "s|^\([^=]*\)=\(.*\)|\1='\2'; export \1|"`; \
+	done; \
+	(cd $(TARGET_SUBDIR)/libphobos && \
+	  $(MAKE) $(BASE_FLAGS_TO_PASS) "AR=$${AR}" "AS=$${AS}" \
+	          "CC=$${CC}" "CXX=$${CXX}" "LD=$${LD}" "NM=$${NM}" \
+	          "RANLIB=$${RANLIB}" \
+	          "DLLTOOL=$${DLLTOOL}" "WINDRES=$${WINDRES}" "WINDMC=$${WINDMC}" \
+	           info) \
+	  || exit 1
+
+@endif target-libphobos
+
+.PHONY: maybe-dvi-target-libphobos dvi-target-libphobos
+maybe-dvi-target-libphobos:
+@if target-libphobos
+maybe-dvi-target-libphobos: dvi-target-libphobos
+
+dvi-target-libphobos: \
+    configure-target-libphobos 
+	@: $(MAKE); $(unstage)
+	@[ -f $(TARGET_SUBDIR)/libphobos/Makefile ] || exit 0; \
+	r=`${PWD_COMMAND}`; export r; \
+	s=`cd $(srcdir); ${PWD_COMMAND}`; export s; \
+	$(NORMAL_TARGET_EXPORTS) \
+	echo "Doing dvi in $(TARGET_SUBDIR)/libphobos"; \
+	for flag in $(EXTRA_TARGET_FLAGS); do \
+	  eval `echo "$$flag" | sed -e "s|^\([^=]*\)=\(.*\)|\1='\2'; export \1|"`; \
+	done; \
+	(cd $(TARGET_SUBDIR)/libphobos && \
+	  $(MAKE) $(BASE_FLAGS_TO_PASS) "AR=$${AR}" "AS=$${AS}" \
+	          "CC=$${CC}" "CXX=$${CXX}" "LD=$${LD}" "NM=$${NM}" \
+	          "RANLIB=$${RANLIB}" \
+	          "DLLTOOL=$${DLLTOOL}" "WINDRES=$${WINDRES}" "WINDMC=$${WINDMC}" \
+	           dvi) \
+	  || exit 1
+
+@endif target-libphobos
+
+.PHONY: maybe-pdf-target-libphobos pdf-target-libphobos
+maybe-pdf-target-libphobos:
+@if target-libphobos
+maybe-pdf-target-libphobos: pdf-target-libphobos
+
+pdf-target-libphobos: \
+    configure-target-libphobos 
+	@: $(MAKE); $(unstage)
+	@[ -f $(TARGET_SUBDIR)/libphobos/Makefile ] || exit 0; \
+	r=`${PWD_COMMAND}`; export r; \
+	s=`cd $(srcdir); ${PWD_COMMAND}`; export s; \
+	$(NORMAL_TARGET_EXPORTS) \
+	echo "Doing pdf in $(TARGET_SUBDIR)/libphobos"; \
+	for flag in $(EXTRA_TARGET_FLAGS); do \
+	  eval `echo "$$flag" | sed -e "s|^\([^=]*\)=\(.*\)|\1='\2'; export \1|"`; \
+	done; \
+	(cd $(TARGET_SUBDIR)/libphobos && \
+	  $(MAKE) $(BASE_FLAGS_TO_PASS) "AR=$${AR}" "AS=$${AS}" \
+	          "CC=$${CC}" "CXX=$${CXX}" "LD=$${LD}" "NM=$${NM}" \
+	          "RANLIB=$${RANLIB}" \
+	          "DLLTOOL=$${DLLTOOL}" "WINDRES=$${WINDRES}" "WINDMC=$${WINDMC}" \
+	           pdf) \
+	  || exit 1
+
+@endif target-libphobos
+
+.PHONY: maybe-html-target-libphobos html-target-libphobos
+maybe-html-target-libphobos:
+@if target-libphobos
+maybe-html-target-libphobos: html-target-libphobos
+
+html-target-libphobos: \
+    configure-target-libphobos 
+	@: $(MAKE); $(unstage)
+	@[ -f $(TARGET_SUBDIR)/libphobos/Makefile ] || exit 0; \
+	r=`${PWD_COMMAND}`; export r; \
+	s=`cd $(srcdir); ${PWD_COMMAND}`; export s; \
+	$(NORMAL_TARGET_EXPORTS) \
+	echo "Doing html in $(TARGET_SUBDIR)/libphobos"; \
+	for flag in $(EXTRA_TARGET_FLAGS); do \
+	  eval `echo "$$flag" | sed -e "s|^\([^=]*\)=\(.*\)|\1='\2'; export \1|"`; \
+	done; \
+	(cd $(TARGET_SUBDIR)/libphobos && \
+	  $(MAKE) $(BASE_FLAGS_TO_PASS) "AR=$${AR}" "AS=$${AS}" \
+	          "CC=$${CC}" "CXX=$${CXX}" "LD=$${LD}" "NM=$${NM}" \
+	          "RANLIB=$${RANLIB}" \
+	          "DLLTOOL=$${DLLTOOL}" "WINDRES=$${WINDRES}" "WINDMC=$${WINDMC}" \
+	           html) \
+	  || exit 1
+
+@endif target-libphobos
+
+.PHONY: maybe-TAGS-target-libphobos TAGS-target-libphobos
+maybe-TAGS-target-libphobos:
+@if target-libphobos
+maybe-TAGS-target-libphobos: TAGS-target-libphobos
+
+TAGS-target-libphobos: \
+    configure-target-libphobos 
+	@: $(MAKE); $(unstage)
+	@[ -f $(TARGET_SUBDIR)/libphobos/Makefile ] || exit 0; \
+	r=`${PWD_COMMAND}`; export r; \
+	s=`cd $(srcdir); ${PWD_COMMAND}`; export s; \
+	$(NORMAL_TARGET_EXPORTS) \
+	echo "Doing TAGS in $(TARGET_SUBDIR)/libphobos"; \
+	for flag in $(EXTRA_TARGET_FLAGS); do \
+	  eval `echo "$$flag" | sed -e "s|^\([^=]*\)=\(.*\)|\1='\2'; export \1|"`; \
+	done; \
+	(cd $(TARGET_SUBDIR)/libphobos && \
+	  $(MAKE) $(BASE_FLAGS_TO_PASS) "AR=$${AR}" "AS=$${AS}" \
+	          "CC=$${CC}" "CXX=$${CXX}" "LD=$${LD}" "NM=$${NM}" \
+	          "RANLIB=$${RANLIB}" \
+	          "DLLTOOL=$${DLLTOOL}" "WINDRES=$${WINDRES}" "WINDMC=$${WINDMC}" \
+	           TAGS) \
+	  || exit 1
+
+@endif target-libphobos
+
+.PHONY: maybe-install-info-target-libphobos install-info-target-libphobos
+maybe-install-info-target-libphobos:
+@if target-libphobos
+maybe-install-info-target-libphobos: install-info-target-libphobos
+
+install-info-target-libphobos: \
+    configure-target-libphobos \
+    info-target-libphobos 
+	@: $(MAKE); $(unstage)
+	@[ -f $(TARGET_SUBDIR)/libphobos/Makefile ] || exit 0; \
+	r=`${PWD_COMMAND}`; export r; \
+	s=`cd $(srcdir); ${PWD_COMMAND}`; export s; \
+	$(NORMAL_TARGET_EXPORTS) \
+	echo "Doing install-info in $(TARGET_SUBDIR)/libphobos"; \
+	for flag in $(EXTRA_TARGET_FLAGS); do \
+	  eval `echo "$$flag" | sed -e "s|^\([^=]*\)=\(.*\)|\1='\2'; export \1|"`; \
+	done; \
+	(cd $(TARGET_SUBDIR)/libphobos && \
+	  $(MAKE) $(BASE_FLAGS_TO_PASS) "AR=$${AR}" "AS=$${AS}" \
+	          "CC=$${CC}" "CXX=$${CXX}" "LD=$${LD}" "NM=$${NM}" \
+	          "RANLIB=$${RANLIB}" \
+	          "DLLTOOL=$${DLLTOOL}" "WINDRES=$${WINDRES}" "WINDMC=$${WINDMC}" \
+	           install-info) \
+	  || exit 1
+
+@endif target-libphobos
+
+.PHONY: maybe-install-pdf-target-libphobos install-pdf-target-libphobos
+maybe-install-pdf-target-libphobos:
+@if target-libphobos
+maybe-install-pdf-target-libphobos: install-pdf-target-libphobos
+
+install-pdf-target-libphobos: \
+    configure-target-libphobos \
+    pdf-target-libphobos 
+	@: $(MAKE); $(unstage)
+	@[ -f $(TARGET_SUBDIR)/libphobos/Makefile ] || exit 0; \
+	r=`${PWD_COMMAND}`; export r; \
+	s=`cd $(srcdir); ${PWD_COMMAND}`; export s; \
+	$(NORMAL_TARGET_EXPORTS) \
+	echo "Doing install-pdf in $(TARGET_SUBDIR)/libphobos"; \
+	for flag in $(EXTRA_TARGET_FLAGS); do \
+	  eval `echo "$$flag" | sed -e "s|^\([^=]*\)=\(.*\)|\1='\2'; export \1|"`; \
+	done; \
+	(cd $(TARGET_SUBDIR)/libphobos && \
+	  $(MAKE) $(BASE_FLAGS_TO_PASS) "AR=$${AR}" "AS=$${AS}" \
+	          "CC=$${CC}" "CXX=$${CXX}" "LD=$${LD}" "NM=$${NM}" \
+	          "RANLIB=$${RANLIB}" \
+	          "DLLTOOL=$${DLLTOOL}" "WINDRES=$${WINDRES}" "WINDMC=$${WINDMC}" \
+	           install-pdf) \
+	  || exit 1
+
+@endif target-libphobos
+
+.PHONY: maybe-install-html-target-libphobos install-html-target-libphobos
+maybe-install-html-target-libphobos:
+@if target-libphobos
+maybe-install-html-target-libphobos: install-html-target-libphobos
+
+install-html-target-libphobos: \
+    configure-target-libphobos \
+    html-target-libphobos 
+	@: $(MAKE); $(unstage)
+	@[ -f $(TARGET_SUBDIR)/libphobos/Makefile ] || exit 0; \
+	r=`${PWD_COMMAND}`; export r; \
+	s=`cd $(srcdir); ${PWD_COMMAND}`; export s; \
+	$(NORMAL_TARGET_EXPORTS) \
+	echo "Doing install-html in $(TARGET_SUBDIR)/libphobos"; \
+	for flag in $(EXTRA_TARGET_FLAGS); do \
+	  eval `echo "$$flag" | sed -e "s|^\([^=]*\)=\(.*\)|\1='\2'; export \1|"`; \
+	done; \
+	(cd $(TARGET_SUBDIR)/libphobos && \
+	  $(MAKE) $(BASE_FLAGS_TO_PASS) "AR=$${AR}" "AS=$${AS}" \
+	          "CC=$${CC}" "CXX=$${CXX}" "LD=$${LD}" "NM=$${NM}" \
+	          "RANLIB=$${RANLIB}" \
+	          "DLLTOOL=$${DLLTOOL}" "WINDRES=$${WINDRES}" "WINDMC=$${WINDMC}" \
+	           install-html) \
+	  || exit 1
+
+@endif target-libphobos
+
+.PHONY: maybe-installcheck-target-libphobos installcheck-target-libphobos
+maybe-installcheck-target-libphobos:
+@if target-libphobos
+maybe-installcheck-target-libphobos: installcheck-target-libphobos
+
+installcheck-target-libphobos: \
+    configure-target-libphobos 
+	@: $(MAKE); $(unstage)
+	@[ -f $(TARGET_SUBDIR)/libphobos/Makefile ] || exit 0; \
+	r=`${PWD_COMMAND}`; export r; \
+	s=`cd $(srcdir); ${PWD_COMMAND}`; export s; \
+	$(NORMAL_TARGET_EXPORTS) \
+	echo "Doing installcheck in $(TARGET_SUBDIR)/libphobos"; \
+	for flag in $(EXTRA_TARGET_FLAGS); do \
+	  eval `echo "$$flag" | sed -e "s|^\([^=]*\)=\(.*\)|\1='\2'; export \1|"`; \
+	done; \
+	(cd $(TARGET_SUBDIR)/libphobos && \
+	  $(MAKE) $(BASE_FLAGS_TO_PASS) "AR=$${AR}" "AS=$${AS}" \
+	          "CC=$${CC}" "CXX=$${CXX}" "LD=$${LD}" "NM=$${NM}" \
+	          "RANLIB=$${RANLIB}" \
+	          "DLLTOOL=$${DLLTOOL}" "WINDRES=$${WINDRES}" "WINDMC=$${WINDMC}" \
+	           installcheck) \
+	  || exit 1
+
+@endif target-libphobos
+
+.PHONY: maybe-mostlyclean-target-libphobos mostlyclean-target-libphobos
+maybe-mostlyclean-target-libphobos:
+@if target-libphobos
+maybe-mostlyclean-target-libphobos: mostlyclean-target-libphobos
+
+mostlyclean-target-libphobos: 
+	@: $(MAKE); $(unstage)
+	@[ -f $(TARGET_SUBDIR)/libphobos/Makefile ] || exit 0; \
+	r=`${PWD_COMMAND}`; export r; \
+	s=`cd $(srcdir); ${PWD_COMMAND}`; export s; \
+	$(NORMAL_TARGET_EXPORTS) \
+	echo "Doing mostlyclean in $(TARGET_SUBDIR)/libphobos"; \
+	for flag in $(EXTRA_TARGET_FLAGS); do \
+	  eval `echo "$$flag" | sed -e "s|^\([^=]*\)=\(.*\)|\1='\2'; export \1|"`; \
+	done; \
+	(cd $(TARGET_SUBDIR)/libphobos && \
+	  $(MAKE) $(BASE_FLAGS_TO_PASS) "AR=$${AR}" "AS=$${AS}" \
+	          "CC=$${CC}" "CXX=$${CXX}" "LD=$${LD}" "NM=$${NM}" \
+	          "RANLIB=$${RANLIB}" \
+	          "DLLTOOL=$${DLLTOOL}" "WINDRES=$${WINDRES}" "WINDMC=$${WINDMC}" \
+	           mostlyclean) \
+	  || exit 1
+
+@endif target-libphobos
+
+.PHONY: maybe-clean-target-libphobos clean-target-libphobos
+maybe-clean-target-libphobos:
+@if target-libphobos
+maybe-clean-target-libphobos: clean-target-libphobos
+
+clean-target-libphobos: 
+	@: $(MAKE); $(unstage)
+	@[ -f $(TARGET_SUBDIR)/libphobos/Makefile ] || exit 0; \
+	r=`${PWD_COMMAND}`; export r; \
+	s=`cd $(srcdir); ${PWD_COMMAND}`; export s; \
+	$(NORMAL_TARGET_EXPORTS) \
+	echo "Doing clean in $(TARGET_SUBDIR)/libphobos"; \
+	for flag in $(EXTRA_TARGET_FLAGS); do \
+	  eval `echo "$$flag" | sed -e "s|^\([^=]*\)=\(.*\)|\1='\2'; export \1|"`; \
+	done; \
+	(cd $(TARGET_SUBDIR)/libphobos && \
+	  $(MAKE) $(BASE_FLAGS_TO_PASS) "AR=$${AR}" "AS=$${AS}" \
+	          "CC=$${CC}" "CXX=$${CXX}" "LD=$${LD}" "NM=$${NM}" \
+	          "RANLIB=$${RANLIB}" \
+	          "DLLTOOL=$${DLLTOOL}" "WINDRES=$${WINDRES}" "WINDMC=$${WINDMC}" \
+	           clean) \
+	  || exit 1
+
+@endif target-libphobos
+
+.PHONY: maybe-distclean-target-libphobos distclean-target-libphobos
+maybe-distclean-target-libphobos:
+@if target-libphobos
+maybe-distclean-target-libphobos: distclean-target-libphobos
+
+distclean-target-libphobos: 
+	@: $(MAKE); $(unstage)
+	@[ -f $(TARGET_SUBDIR)/libphobos/Makefile ] || exit 0; \
+	r=`${PWD_COMMAND}`; export r; \
+	s=`cd $(srcdir); ${PWD_COMMAND}`; export s; \
+	$(NORMAL_TARGET_EXPORTS) \
+	echo "Doing distclean in $(TARGET_SUBDIR)/libphobos"; \
+	for flag in $(EXTRA_TARGET_FLAGS); do \
+	  eval `echo "$$flag" | sed -e "s|^\([^=]*\)=\(.*\)|\1='\2'; export \1|"`; \
+	done; \
+	(cd $(TARGET_SUBDIR)/libphobos && \
+	  $(MAKE) $(BASE_FLAGS_TO_PASS) "AR=$${AR}" "AS=$${AS}" \
+	          "CC=$${CC}" "CXX=$${CXX}" "LD=$${LD}" "NM=$${NM}" \
+	          "RANLIB=$${RANLIB}" \
+	          "DLLTOOL=$${DLLTOOL}" "WINDRES=$${WINDRES}" "WINDMC=$${WINDMC}" \
+	           distclean) \
+	  || exit 1
+
+@endif target-libphobos
+
+.PHONY: maybe-maintainer-clean-target-libphobos maintainer-clean-target-libphobos
+maybe-maintainer-clean-target-libphobos:
+@if target-libphobos
+maybe-maintainer-clean-target-libphobos: maintainer-clean-target-libphobos
+
+maintainer-clean-target-libphobos: 
+	@: $(MAKE); $(unstage)
+	@[ -f $(TARGET_SUBDIR)/libphobos/Makefile ] || exit 0; \
+	r=`${PWD_COMMAND}`; export r; \
+	s=`cd $(srcdir); ${PWD_COMMAND}`; export s; \
+	$(NORMAL_TARGET_EXPORTS) \
+	echo "Doing maintainer-clean in $(TARGET_SUBDIR)/libphobos"; \
+	for flag in $(EXTRA_TARGET_FLAGS); do \
+	  eval `echo "$$flag" | sed -e "s|^\([^=]*\)=\(.*\)|\1='\2'; export \1|"`; \
+	done; \
+	(cd $(TARGET_SUBDIR)/libphobos && \
+	  $(MAKE) $(BASE_FLAGS_TO_PASS) "AR=$${AR}" "AS=$${AS}" \
+	          "CC=$${CC}" "CXX=$${CXX}" "LD=$${LD}" "NM=$${NM}" \
+	          "RANLIB=$${RANLIB}" \
+	          "DLLTOOL=$${DLLTOOL}" "WINDRES=$${WINDRES}" "WINDMC=$${WINDMC}" \
+	           maintainer-clean) \
+	  || exit 1
+
+@endif target-libphobos
+
+
+
+
+
 .PHONY: configure-target-libtermcap maybe-configure-target-libtermcap
 maybe-configure-target-libtermcap:
 @if gcc-bootstrap
@@ -51864,6 +52348,14 @@ check-gcc-brig:
 	(cd gcc && $(MAKE) $(GCC_FLAGS_TO_PASS) check-brig);
 check-brig: check-gcc-brig check-target-libhsail-rt
 
+.PHONY: check-gcc-d check-d
+check-gcc-d:
+	r=`${PWD_COMMAND}`; export r; \
+	s=`cd $(srcdir); ${PWD_COMMAND}`; export s; \
+	$(HOST_EXPORTS) \
+	(cd gcc && $(MAKE) $(GCC_FLAGS_TO_PASS) check-d);
+check-d: check-gcc-d check-target-libphobos
+
 
 # The gcc part of install-no-fixedincludes, which relies on an intimate
 # knowledge of how a number of gcc internal targets (inter)operate.  Delegate.
@@ -54742,6 +55234,7 @@ configure-target-libgfortran: stage_last
 configure-target-libobjc: stage_last
 configure-target-libgo: stage_last
 configure-target-libhsail-rt: stage_last
+configure-target-libphobos: stage_last
 configure-target-libtermcap: stage_last
 configure-target-winsup: stage_last
 configure-target-libgloss: stage_last
@@ -54777,6 +55270,7 @@ configure-target-libgfortran: maybe-all-gcc
 configure-target-libobjc: maybe-all-gcc
 configure-target-libgo: maybe-all-gcc
 configure-target-libhsail-rt: maybe-all-gcc
+configure-target-libphobos: maybe-all-gcc
 configure-target-libtermcap: maybe-all-gcc
 configure-target-winsup: maybe-all-gcc
 configure-target-libgloss: maybe-all-gcc
@@ -55803,6 +56297,8 @@ configure-target-libgo: maybe-all-target-libstdc++-v3
 all-target-libgo: maybe-all-target-libbacktrace
 all-target-libgo: maybe-all-target-libffi
 all-target-libgo: maybe-all-target-libatomic
+configure-target-libphobos: maybe-configure-target-zlib
+all-target-libphobos: maybe-all-target-zlib
 configure-target-libstdc++-v3: maybe-configure-target-libgomp
 
 configure-stage1-target-libstdc++-v3: maybe-configure-stage1-target-libgomp
@@ -55930,6 +56426,7 @@ configure-target-libgfortran: maybe-all-target-libgcc
 configure-target-libobjc: maybe-all-target-libgcc
 configure-target-libgo: maybe-all-target-libgcc
 configure-target-libhsail-rt: maybe-all-target-libgcc
+configure-target-libphobos: maybe-all-target-libgcc
 configure-target-libtermcap: maybe-all-target-libgcc
 configure-target-winsup: maybe-all-target-libgcc
 configure-target-libgloss: maybe-all-target-libgcc
@@ -55973,6 +56470,8 @@ configure-target-libgo: maybe-all-target-newlib maybe-all-target-libgloss
 
 configure-target-libhsail-rt: maybe-all-target-newlib maybe-all-target-libgloss
 
+configure-target-libphobos: maybe-all-target-newlib maybe-all-target-libgloss
+
 configure-target-libtermcap: maybe-all-target-newlib maybe-all-target-libgloss
 
 configure-target-winsup: maybe-all-target-newlib maybe-all-target-libgloss
--- a/Makefile.tpl
+++ b/Makefile.tpl
@@ -159,6 +159,7 @@ BUILD_EXPORTS = \
 	GFORTRAN="$(GFORTRAN_FOR_BUILD)"; export GFORTRAN; \
 	GOC="$(GOC_FOR_BUILD)"; export GOC; \
 	GOCFLAGS="$(GOCFLAGS_FOR_BUILD)"; export GOCFLAGS; \
+	GDC="$(GDC_FOR_BUILD)"; export GDC; \
 	DLLTOOL="$(DLLTOOL_FOR_BUILD)"; export DLLTOOL; \
 	LD="$(LD_FOR_BUILD)"; export LD; \
 	LDFLAGS="$(LDFLAGS_FOR_BUILD)"; export LDFLAGS; \
@@ -195,6 +196,7 @@ HOST_EXPORTS = \
 	CXXFLAGS="$(CXXFLAGS)"; export CXXFLAGS; \
 	GFORTRAN="$(GFORTRAN)"; export GFORTRAN; \
 	GOC="$(GOC)"; export GOC; \
+	GDC="$(GDC)"; export GDC; \
 	AR="$(AR)"; export AR; \
 	AS="$(AS)"; export AS; \
 	CC_FOR_BUILD="$(CC_FOR_BUILD)"; export CC_FOR_BUILD; \
@@ -281,6 +283,7 @@ BASE_TARGET_EXPORTS = \
 	CXXFLAGS="$(CXXFLAGS_FOR_TARGET)"; export CXXFLAGS; \
 	GFORTRAN="$(GFORTRAN_FOR_TARGET) $(XGCC_FLAGS_FOR_TARGET) $$TFLAGS"; export GFORTRAN; \
 	GOC="$(GOC_FOR_TARGET) $(XGCC_FLAGS_FOR_TARGET) $$TFLAGS"; export GOC; \
+	GDC="$(GDC_FOR_TARGET) $(XGCC_FLAGS_FOR_TARGET) $$TFLAGS"; export GDC; \
 	DLLTOOL="$(DLLTOOL_FOR_TARGET)"; export DLLTOOL; \
 	LD="$(COMPILER_LD_FOR_TARGET)"; export LD; \
 	LDFLAGS="$(LDFLAGS_FOR_TARGET)"; export LDFLAGS; \
@@ -345,6 +348,7 @@ CXX_FOR_BUILD = @CXX_FOR_BUILD@
 DLLTOOL_FOR_BUILD = @DLLTOOL_FOR_BUILD@
 GFORTRAN_FOR_BUILD = @GFORTRAN_FOR_BUILD@
 GOC_FOR_BUILD = @GOC_FOR_BUILD@
+GDC_FOR_BUILD = @GDC_FOR_BUILD@
 LDFLAGS_FOR_BUILD = @LDFLAGS_FOR_BUILD@
 LD_FOR_BUILD = @LD_FOR_BUILD@
 NM_FOR_BUILD = @NM_FOR_BUILD@
@@ -484,6 +488,7 @@ CXX_FOR_TARGET=$(STAGE_CC_WRAPPER) @CXX_FOR_TARGET@
 RAW_CXX_FOR_TARGET=$(STAGE_CC_WRAPPER) @RAW_CXX_FOR_TARGET@
 GFORTRAN_FOR_TARGET=$(STAGE_CC_WRAPPER) @GFORTRAN_FOR_TARGET@
 GOC_FOR_TARGET=$(STAGE_CC_WRAPPER) @GOC_FOR_TARGET@
+GDC_FOR_TARGET=$(STAGE_CC_WRAPPER) @GDC_FOR_TARGET@
 DLLTOOL_FOR_TARGET=@DLLTOOL_FOR_TARGET@
 LD_FOR_TARGET=@LD_FOR_TARGET@
 
@@ -609,6 +614,7 @@ EXTRA_HOST_FLAGS = \
 	'DLLTOOL=$(DLLTOOL)' \
 	'GFORTRAN=$(GFORTRAN)' \
 	'GOC=$(GOC)' \
+	'GDC=$(GDC)' \
 	'LD=$(LD)' \
 	'LIPO=$(LIPO)' \
 	'NM=$(NM)' \
@@ -665,6 +671,7 @@ EXTRA_TARGET_FLAGS = \
 	'GFORTRAN=$$(GFORTRAN_FOR_TARGET) $$(XGCC_FLAGS_FOR_TARGET) $$(TFLAGS)' \
 	'GOC=$$(GOC_FOR_TARGET) $$(XGCC_FLAGS_FOR_TARGET) $$(TFLAGS)' \
 	'GOCFLAGS=$$(GOCFLAGS_FOR_TARGET)' \
+	'GDC=$$(GDC_FOR_TARGET) $$(XGCC_FLAGS_FOR_TARGET) $$(TFLAGS)' \
 	'LD=$(COMPILER_LD_FOR_TARGET)' \
 	'LDFLAGS=$$(LDFLAGS_FOR_TARGET)' \
 	'LIBCFLAGS=$$(LIBCFLAGS_FOR_TARGET)' \
--- a/config-ml.in
+++ b/config-ml.in
@@ -512,6 +512,7 @@ multi-do:
 				prefix="$(prefix)" \
 				exec_prefix="$(exec_prefix)" \
 				GOCFLAGS="$(GOCFLAGS) $${flags}" \
+				GDCFLAGS="$(GDCFLAGS) $${flags}" \
 				CXXFLAGS="$(CXXFLAGS) $${flags}" \
 				LIBCFLAGS="$(LIBCFLAGS) $${flags}" \
 				LIBCXXFLAGS="$(LIBCXXFLAGS) $${flags}" \
@@ -745,7 +746,7 @@ if [ -n "${multidirs}" ] && [ -z "${ml_norecursion}" ]; then
         break
       fi
     done
-    ml_config_env='CC="${CC_}$flags" CXX="${CXX_}$flags" F77="${F77_}$flags" GFORTRAN="${GFORTRAN_}$flags" GOC="${GOC_}$flags"'
+    ml_config_env='CC="${CC_}$flags" CXX="${CXX_}$flags" F77="${F77_}$flags" GFORTRAN="${GFORTRAN_}$flags" GOC="${GOC_}$flags" GDC="${GDC_}$flags"'
 
     if [ "${with_target_subdir}" = "." ]; then
 	CC_=$CC' '
@@ -753,6 +754,7 @@ if [ -n "${multidirs}" ] && [ -z "${ml_norecursion}" ]; then
 	F77_=$F77' '
 	GFORTRAN_=$GFORTRAN' '
 	GOC_=$GOC' '
+	GDC_=$GDC' '
     else
 	# Create a regular expression that matches any string as long
 	# as ML_POPDIR.
@@ -817,6 +819,18 @@ if [ -n "${multidirs}" ] && [ -z "${ml_norecursion}" ]; then
 	  esac
 	done
 
+	GDC_=
+	for arg in ${GDC}; do
+	  case $arg in
+	  -[BIL]"${ML_POPDIR}"/*)
+	    GDC_="${GDC_}"`echo "X${arg}" | sed -n "s/X\\(-[BIL]${popdir_rx}\\).*/\\1/p"`/${ml_dir}`echo "X${arg}" | sed -n "s/X-[BIL]${popdir_rx}\\(.*\\)/\\1/p"`' ' ;;
+	  "${ML_POPDIR}"/*)
+	    GDC_="${GDC_}"`echo "X${arg}" | sed -n "s/X\\(${popdir_rx}\\).*/\\1/p"`/${ml_dir}`echo "X${arg}" | sed -n "s/X${popdir_rx}\\(.*\\)/\\1/p"`' ' ;;
+	  *)
+	    GDC_="${GDC_}${arg} " ;;
+	  esac
+	done
+
 	if test "x${LD_LIBRARY_PATH+set}" = xset; then
 	  LD_LIBRARY_PATH_=
 	  for arg in `echo "$LD_LIBRARY_PATH" | tr ':' ' '`; do
--- a/config/multi.m4
+++ b/config/multi.m4
@@ -64,4 +64,5 @@ multi_basedir="$multi_basedir"
 CONFIG_SHELL=${CONFIG_SHELL-/bin/sh}
 CC="$CC"
 CXX="$CXX"
-GFORTRAN="$GFORTRAN"])])dnl
+GFORTRAN="$GFORTRAN"
+GDC="$GDC"])])dnl
--- a/configure
+++ b/configure
@@ -581,6 +581,7 @@ LD_FOR_TARGET
 DLLTOOL_FOR_TARGET
 AS_FOR_TARGET
 AR_FOR_TARGET
+GDC_FOR_TARGET
 GOC_FOR_TARGET
 GFORTRAN_FOR_TARGET
 GCC_FOR_TARGET
@@ -613,6 +614,7 @@ RANLIB_FOR_BUILD
 NM_FOR_BUILD
 LD_FOR_BUILD
 LDFLAGS_FOR_BUILD
+GDC_FOR_BUILD
 GOC_FOR_BUILD
 GFORTRAN_FOR_BUILD
 DLLTOOL_FOR_BUILD
@@ -830,6 +832,7 @@ CXX_FOR_TARGET
 GCC_FOR_TARGET
 GFORTRAN_FOR_TARGET
 GOC_FOR_TARGET
+GDC_FOR_TARGET
 AR_FOR_TARGET
 AS_FOR_TARGET
 DLLTOOL_FOR_TARGET
@@ -1613,6 +1616,8 @@ Some influential environment variables:
               GFORTRAN for the target
   GOC_FOR_TARGET
               GOC for the target
+  GDC_FOR_TARGET
+              GDC for the target
   AR_FOR_TARGET
               AR for the target
   AS_FOR_TARGET
@@ -2766,7 +2771,8 @@ target_libraries="target-libgcc \
 		target-libffi \
 		target-libobjc \
 		target-libada \
-		target-libgo"
+		target-libgo \
+		target-libphobos"
 
 # these tools are built using the target libraries, and are intended to
 # run only in the target environment
@@ -3966,6 +3972,7 @@ if test "${build}" != "${host}" ; then
   CXX_FOR_BUILD=${CXX_FOR_BUILD-g++}
   GFORTRAN_FOR_BUILD=${GFORTRAN_FOR_BUILD-gfortran}
   GOC_FOR_BUILD=${GOC_FOR_BUILD-gccgo}
+  GDC_FOR_BUILD=${GDC_FOR_BUILD-gdc}
   DLLTOOL_FOR_BUILD=${DLLTOOL_FOR_BUILD-dlltool}
   LD_FOR_BUILD=${LD_FOR_BUILD-ld}
   NM_FOR_BUILD=${NM_FOR_BUILD-nm}
@@ -3979,6 +3986,7 @@ else
   CXX_FOR_BUILD="\$(CXX)"
   GFORTRAN_FOR_BUILD="\$(GFORTRAN)"
   GOC_FOR_BUILD="\$(GOC)"
+  GDC_FOR_BUILD="\$(GDC)"
   DLLTOOL_FOR_BUILD="\$(DLLTOOL)"
   LD_FOR_BUILD="\$(LD)"
   NM_FOR_BUILD="\$(NM)"
@@ -7631,6 +7639,7 @@ done
 
 
 
+
 # Generate default definitions for YACC, M4, LEX and other programs that run
 # on the build machine.  These are used if the Makefile can't locate these
 # programs in objdir.
@@ -10685,6 +10694,167 @@ fi
 
 
 
+if test -n "$GDC_FOR_TARGET"; then
+  ac_cv_prog_GDC_FOR_TARGET=$GDC_FOR_TARGET
+elif test -n "$ac_cv_prog_GDC_FOR_TARGET"; then
+  GDC_FOR_TARGET=$ac_cv_prog_GDC_FOR_TARGET
+fi
+
+if test -n "$ac_cv_prog_GDC_FOR_TARGET"; then
+  for ncn_progname in gdc; do
+    # Extract the first word of "${ncn_progname}", so it can be a program name with args.
+set dummy ${ncn_progname}; ac_word=$2
+{ $as_echo "$as_me:${as_lineno-$LINENO}: checking for $ac_word" >&5
+$as_echo_n "checking for $ac_word... " >&6; }
+if test "${ac_cv_prog_GDC_FOR_TARGET+set}" = set; then :
+  $as_echo_n "(cached) " >&6
+else
+  if test -n "$GDC_FOR_TARGET"; then
+  ac_cv_prog_GDC_FOR_TARGET="$GDC_FOR_TARGET" # Let the user override the test.
+else
+as_save_IFS=$IFS; IFS=$PATH_SEPARATOR
+for as_dir in $PATH
+do
+  IFS=$as_save_IFS
+  test -z "$as_dir" && as_dir=.
+    for ac_exec_ext in '' $ac_executable_extensions; do
+  if { test -f "$as_dir/$ac_word$ac_exec_ext" && $as_test_x "$as_dir/$ac_word$ac_exec_ext"; }; then
+    ac_cv_prog_GDC_FOR_TARGET="${ncn_progname}"
+    $as_echo "$as_me:${as_lineno-$LINENO}: found $as_dir/$ac_word$ac_exec_ext" >&5
+    break 2
+  fi
+done
+  done
+IFS=$as_save_IFS
+
+fi
+fi
+GDC_FOR_TARGET=$ac_cv_prog_GDC_FOR_TARGET
+if test -n "$GDC_FOR_TARGET"; then
+  { $as_echo "$as_me:${as_lineno-$LINENO}: result: $GDC_FOR_TARGET" >&5
+$as_echo "$GDC_FOR_TARGET" >&6; }
+else
+  { $as_echo "$as_me:${as_lineno-$LINENO}: result: no" >&5
+$as_echo "no" >&6; }
+fi
+
+
+  done
+fi
+
+if test -z "$ac_cv_prog_GDC_FOR_TARGET" && test -n "$with_build_time_tools"; then
+  for ncn_progname in gdc; do
+    { $as_echo "$as_me:${as_lineno-$LINENO}: checking for ${ncn_progname} in $with_build_time_tools" >&5
+$as_echo_n "checking for ${ncn_progname} in $with_build_time_tools... " >&6; }
+    if test -x $with_build_time_tools/${ncn_progname}; then
+      ac_cv_prog_GDC_FOR_TARGET=$with_build_time_tools/${ncn_progname}
+      { $as_echo "$as_me:${as_lineno-$LINENO}: result: yes" >&5
+$as_echo "yes" >&6; }
+      break
+    else
+      { $as_echo "$as_me:${as_lineno-$LINENO}: result: no" >&5
+$as_echo "no" >&6; }
+    fi
+  done
+fi
+
+if test -z "$ac_cv_prog_GDC_FOR_TARGET"; then
+  for ncn_progname in gdc; do
+    if test -n "$ncn_target_tool_prefix"; then
+      # Extract the first word of "${ncn_target_tool_prefix}${ncn_progname}", so it can be a program name with args.
+set dummy ${ncn_target_tool_prefix}${ncn_progname}; ac_word=$2
+{ $as_echo "$as_me:${as_lineno-$LINENO}: checking for $ac_word" >&5
+$as_echo_n "checking for $ac_word... " >&6; }
+if test "${ac_cv_prog_GDC_FOR_TARGET+set}" = set; then :
+  $as_echo_n "(cached) " >&6
+else
+  if test -n "$GDC_FOR_TARGET"; then
+  ac_cv_prog_GDC_FOR_TARGET="$GDC_FOR_TARGET" # Let the user override the test.
+else
+as_save_IFS=$IFS; IFS=$PATH_SEPARATOR
+for as_dir in $PATH
+do
+  IFS=$as_save_IFS
+  test -z "$as_dir" && as_dir=.
+    for ac_exec_ext in '' $ac_executable_extensions; do
+  if { test -f "$as_dir/$ac_word$ac_exec_ext" && $as_test_x "$as_dir/$ac_word$ac_exec_ext"; }; then
+    ac_cv_prog_GDC_FOR_TARGET="${ncn_target_tool_prefix}${ncn_progname}"
+    $as_echo "$as_me:${as_lineno-$LINENO}: found $as_dir/$ac_word$ac_exec_ext" >&5
+    break 2
+  fi
+done
+  done
+IFS=$as_save_IFS
+
+fi
+fi
+GDC_FOR_TARGET=$ac_cv_prog_GDC_FOR_TARGET
+if test -n "$GDC_FOR_TARGET"; then
+  { $as_echo "$as_me:${as_lineno-$LINENO}: result: $GDC_FOR_TARGET" >&5
+$as_echo "$GDC_FOR_TARGET" >&6; }
+else
+  { $as_echo "$as_me:${as_lineno-$LINENO}: result: no" >&5
+$as_echo "no" >&6; }
+fi
+
+
+    fi
+    if test -z "$ac_cv_prog_GDC_FOR_TARGET" && test $build = $target ; then
+      # Extract the first word of "${ncn_progname}", so it can be a program name with args.
+set dummy ${ncn_progname}; ac_word=$2
+{ $as_echo "$as_me:${as_lineno-$LINENO}: checking for $ac_word" >&5
+$as_echo_n "checking for $ac_word... " >&6; }
+if test "${ac_cv_prog_GDC_FOR_TARGET+set}" = set; then :
+  $as_echo_n "(cached) " >&6
+else
+  if test -n "$GDC_FOR_TARGET"; then
+  ac_cv_prog_GDC_FOR_TARGET="$GDC_FOR_TARGET" # Let the user override the test.
+else
+as_save_IFS=$IFS; IFS=$PATH_SEPARATOR
+for as_dir in $PATH
+do
+  IFS=$as_save_IFS
+  test -z "$as_dir" && as_dir=.
+    for ac_exec_ext in '' $ac_executable_extensions; do
+  if { test -f "$as_dir/$ac_word$ac_exec_ext" && $as_test_x "$as_dir/$ac_word$ac_exec_ext"; }; then
+    ac_cv_prog_GDC_FOR_TARGET="${ncn_progname}"
+    $as_echo "$as_me:${as_lineno-$LINENO}: found $as_dir/$ac_word$ac_exec_ext" >&5
+    break 2
+  fi
+done
+  done
+IFS=$as_save_IFS
+
+fi
+fi
+GDC_FOR_TARGET=$ac_cv_prog_GDC_FOR_TARGET
+if test -n "$GDC_FOR_TARGET"; then
+  { $as_echo "$as_me:${as_lineno-$LINENO}: result: $GDC_FOR_TARGET" >&5
+$as_echo "$GDC_FOR_TARGET" >&6; }
+else
+  { $as_echo "$as_me:${as_lineno-$LINENO}: result: no" >&5
+$as_echo "no" >&6; }
+fi
+
+
+    fi
+    test -n "$ac_cv_prog_GDC_FOR_TARGET" && break
+  done
+fi
+
+if test -z "$ac_cv_prog_GDC_FOR_TARGET" ; then
+  set dummy gdc
+  if test $build = $target ; then
+    GDC_FOR_TARGET="$2"
+  else
+    GDC_FOR_TARGET="${ncn_target_tool_prefix}$2"
+  fi
+else
+  GDC_FOR_TARGET="$ac_cv_prog_GDC_FOR_TARGET"
+fi
+
+
+
 cat > conftest.c << \EOF
 #ifdef __GNUC__
   gcc_yay;
@@ -14081,6 +14251,51 @@ $as_echo "pre-installed" >&6; }
   fi
 fi
 
+{ $as_echo "$as_me:${as_lineno-$LINENO}: checking where to find the target gdc" >&5
+$as_echo_n "checking where to find the target gdc... " >&6; }
+if test "x${build}" != "x${host}" ; then
+  if expr "x$GDC_FOR_TARGET" : "x/" > /dev/null; then
+    # We already found the complete path
+    ac_dir=`dirname $GDC_FOR_TARGET`
+    { $as_echo "$as_me:${as_lineno-$LINENO}: result: pre-installed in $ac_dir" >&5
+$as_echo "pre-installed in $ac_dir" >&6; }
+  else
+    # Canadian cross, just use what we found
+    { $as_echo "$as_me:${as_lineno-$LINENO}: result: pre-installed" >&5
+$as_echo "pre-installed" >&6; }
+  fi
+else
+  ok=yes
+  case " ${configdirs} " in
+    *" gcc "*) ;;
+    *) ok=no ;;
+  esac
+  case ,${enable_languages}, in
+    *,d,*) ;;
+    *) ok=no ;;
+  esac
+  if test $ok = yes; then
+    # An in-tree tool is available and we can use it
+    GDC_FOR_TARGET='$$r/$(HOST_SUBDIR)/gcc/gdc -B$$r/$(HOST_SUBDIR)/gcc/'
+    { $as_echo "$as_me:${as_lineno-$LINENO}: result: just compiled" >&5
+$as_echo "just compiled" >&6; }
+  elif expr "x$GDC_FOR_TARGET" : "x/" > /dev/null; then
+    # We already found the complete path
+    ac_dir=`dirname $GDC_FOR_TARGET`
+    { $as_echo "$as_me:${as_lineno-$LINENO}: result: pre-installed in $ac_dir" >&5
+$as_echo "pre-installed in $ac_dir" >&6; }
+  elif test "x$target" = "x$host"; then
+    # We can use an host tool
+    GDC_FOR_TARGET='$(GDC)'
+    { $as_echo "$as_me:${as_lineno-$LINENO}: result: host tool" >&5
+$as_echo "host tool" >&6; }
+  else
+    # We need a cross tool
+    { $as_echo "$as_me:${as_lineno-$LINENO}: result: pre-installed" >&5
+$as_echo "pre-installed" >&6; }
+  fi
+fi
+
 { $as_echo "$as_me:${as_lineno-$LINENO}: checking where to find the target ld" >&5
 $as_echo_n "checking where to find the target ld... " >&6; }
 if test "x${build}" != "x${host}" ; then
--- a/configure.ac
+++ b/configure.ac
@@ -165,7 +165,8 @@ target_libraries="target-libgcc \
 		target-libffi \
 		target-libobjc \
 		target-libada \
-		target-libgo"
+		target-libgo \
+		target-libphobos"
 
 # these tools are built using the target libraries, and are intended to
 # run only in the target environment
@@ -1275,6 +1276,7 @@ if test "${build}" != "${host}" ; then
   CXX_FOR_BUILD=${CXX_FOR_BUILD-g++}
   GFORTRAN_FOR_BUILD=${GFORTRAN_FOR_BUILD-gfortran}
   GOC_FOR_BUILD=${GOC_FOR_BUILD-gccgo}
+  GDC_FOR_BUILD=${GDC_FOR_BUILD-gdc}
   DLLTOOL_FOR_BUILD=${DLLTOOL_FOR_BUILD-dlltool}
   LD_FOR_BUILD=${LD_FOR_BUILD-ld}
   NM_FOR_BUILD=${NM_FOR_BUILD-nm}
@@ -1288,6 +1290,7 @@ else
   CXX_FOR_BUILD="\$(CXX)"
   GFORTRAN_FOR_BUILD="\$(GFORTRAN)"
   GOC_FOR_BUILD="\$(GOC)"
+  GDC_FOR_BUILD="\$(GDC)"
   DLLTOOL_FOR_BUILD="\$(DLLTOOL)"
   LD_FOR_BUILD="\$(LD)"
   NM_FOR_BUILD="\$(NM)"
@@ -3217,6 +3220,7 @@ AC_SUBST(CXX_FOR_BUILD)
 AC_SUBST(DLLTOOL_FOR_BUILD)
 AC_SUBST(GFORTRAN_FOR_BUILD)
 AC_SUBST(GOC_FOR_BUILD)
+AC_SUBST(GDC_FOR_BUILD)
 AC_SUBST(LDFLAGS_FOR_BUILD)
 AC_SUBST(LD_FOR_BUILD)
 AC_SUBST(NM_FOR_BUILD)
@@ -3326,6 +3330,7 @@ NCN_STRICT_CHECK_TARGET_TOOLS(CXX_FOR_TARGET, c++ g++ cxx gxx)
 NCN_STRICT_CHECK_TARGET_TOOLS(GCC_FOR_TARGET, gcc, ${CC_FOR_TARGET})
 NCN_STRICT_CHECK_TARGET_TOOLS(GFORTRAN_FOR_TARGET, gfortran)
 NCN_STRICT_CHECK_TARGET_TOOLS(GOC_FOR_TARGET, gccgo)
+NCN_STRICT_CHECK_TARGET_TOOLS(GDC_FOR_TARGET, gdc)
 
 ACX_CHECK_INSTALLED_TARGET_TOOL(AR_FOR_TARGET, ar)
 ACX_CHECK_INSTALLED_TARGET_TOOL(AS_FOR_TARGET, as)
@@ -3359,6 +3364,8 @@ GCC_TARGET_TOOL(gfortran, GFORTRAN_FOR_TARGET, GFORTRAN,
 		[gcc/gfortran -B$$r/$(HOST_SUBDIR)/gcc/], fortran)
 GCC_TARGET_TOOL(gccgo, GOC_FOR_TARGET, GOC,
 		[gcc/gccgo -B$$r/$(HOST_SUBDIR)/gcc/], go)
+GCC_TARGET_TOOL(gdc, GDC_FOR_TARGET, GDC,
+		[gcc/gdc -B$$r/$(HOST_SUBDIR)/gcc/], d)
 GCC_TARGET_TOOL(ld, LD_FOR_TARGET, LD, [ld/ld-new])
 GCC_TARGET_TOOL(lipo, LIPO_FOR_TARGET, LIPO)
 GCC_TARGET_TOOL(nm, NM_FOR_TARGET, NM, [binutils/nm-new])
