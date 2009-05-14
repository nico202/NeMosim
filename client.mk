# 
# Build C-API for nemo client
#
# The build should work under Linux and Win32/MinGW
#
# Author: Andreas Fidjeland
#

m_src := nsSetHost nsSetPort nsStart nsRun nsTerminate nsEnableSTDP nsDisableSTDP nsApplySTDP
mex_src_dir := src/client-api
client_dist := client-dist
build_dir := $(client_dist)/build
hs_build_dir := dist/build
hs_stubs := dist/build/Network
ghc_include := $(shell ghc --print-libdir)/include

ifeq ($(OSTYPE),msys)
so := dll
obj := obj
mex_ext := dll
else
so := so
obj := o
mex_ext := mexglx
endif

version :=$(shell util/version.hs)

m_files := $(patsubst %,$(mex_src_dir)/matlab/%.m,$(m_src))
mex_files := $(addprefix $(build_dir)/,$(addsuffix .$(mex_ext),nsStart_aux nsRun_aux nsTerminate_aux))
client_so := $(build_dir)/libnemoclient.$(so)


all: $(mex_files)

packages := network network-bytestring bytestring binary haskell98 \
		containers array parallel


# wrapper object for haskell FFI'd code 
$(build_dir)/client.$(obj): $(mex_src_dir)/client.c $(hs_stubs)/ClientFFI_stub.h 
	mkdir -p $(build_dir)
	gcc -I$(ghc_include) -I$(hs_stubs) -c -fPIC -o $@ $<

# Create shared object against which various external interfaces can link
$(client_so): \
		$(hs_stubs)/ClientFFI_stub.o \
		$(build_dir)/client.$(obj) \
		$(hs_build_dir)/libHSnemo-$(version).a
	ghc -v $(addprefix -package ,$(packages)) -shared \
		-L$(hs_build_dir) -lHSnemo-$(version) $^ -o $@
	strip $@


# Create matlab objects
mex_includes :=  -I$(ghc_include) -I$(build_dir) -I$(hs_stubs)

# TODO: have configure pick up on this
ifeq ($(OSTYPE),msys)
CC =/mingw/bin/gcc
CXX=/mingw/bin/g++
DLLTOOL=/mingw/bin/dlltool

MEXMINGW=/usr/local/src/mex-mingw32-0.1
WMEXINC=$(MEXMINGW)/mexinc

WMEXLIBDIR=$(MEXMINGW)/mexlib

WMEXCFLAGS=\
	-DMATLAB_WMEX_FILE -I$(WMEXINC) -O3 \
	-malign-double -fno-exceptions

WMEXLIBS=\
	$(WMEXLIBDIR)/fixup.o \
	$(WMEXLIBDIR)/mexlib1.lib \
	$(WMEXLIBDIR)/mexlib2.lib \
	$(WMEXLIBDIR)/mexlib3.lib

$(build_dir)/%.dll: $(mex_src_dir)/%.c $(client_so)
	$(CC) -c $(WMEXCFLAGS) $(CFLAGS) $(mex_includes) -I/c/MinGW/include $< -o $(build_dir)/$*.o
	$(CC) -shared $(WMEXLIBDIR)/mex.def -o $@ -s \
		$(build_dir)/$*.o \
		$(build_dir)/libnemoclient.dll.a \
		/c/MinGW/lib/libws2_32.a \
		$(WMEXLIBS)

#	cmd //c mex.bat -f util/mexopts.bat $(mex_includes) -outdir $(build_dir) \
		$< $(build_dir)/libnemoclient.dll.a /c/MinGW/lib/libws2_32.a
else
$(build_dir)/%.$(mex_ext): $(mex_src_dir)/%.c $(client_so)
	matlab-mex $(mex_includes) -lnemoclient -L$(build_dir) -outdir $(build_dir) $<
endif


#
# Documentation
#

doc_dir := $(client_dist)/doc

# function API descriptions are found in the m-files
$(doc_dir)/%.txt: $(mex_src_dir)/matlab/%.m
	mkdir -p $(dir $@)
	util/extract_matlab_docstring.pl $< > $@


# A description of the whole matlab client is found in the wiki
$(doc_dir)/manual.tex: \
		doc/wiki/MatlabAPI \
		$(mex_src_dir)/matlab/manual_stylesheet.tex \
		$(patsubst %,$(doc_dir)/%.txt,$(m_src))
	cp --target-directory $(doc_dir) $(wordlist 1,2,$^)
	# cp --target-directory $(doc_dir) doc/wiki/MatlabAPI
	rst2latex --use-latex-docinfo --use-latex-toc --no-section-numbering \
		--stylesheet=manual_stylesheet.tex \
		$(doc_dir)/MatlabAPI > $@

$(doc_dir)/manual.pdf: $(doc_dir)/manual.tex
	(cd $(dir $<); pdflatex $(notdir $<); cd ..)

.PHONY: doc
doc: $(client_dist)/doc/manual.pdf

.PHONY: doc-clean
doc-clean:
	rm -f $(addprefix $(doc_dir)/, *.tex *.log *.aux *.txt *.out *.pdf MatlabAPI)


#
# Distribution
#

ifeq ($(OSTYPE),msys)
matlab_dist := nemo-client-win32-$(strip $(version))
else
matlab_dist := nemo-client-glx-$(strip $(version))
endif
.PHONY: matlab-dist
matlab-dist: $(mex_files) $(client_so) $(m_files) $(doc_dir)/manual.pdf
	rm -f $(client_dist)/matlab/latest
	mkdir -p $(client_dist)/matlab/$(matlab_dist)
	(cd $(client_dist)/matlab; ln -s $(matlab_dist) latest; cd ../..)
	cp --target-directory $(client_dist)/matlab/$(matlab_dist) $^
	(cd $(client_dist)/matlab; zip -r $(matlab_dist).zip $(matlab_dist); cd ../..)
	


.PHONY: clean
clean: doc-clean
	rm -f $(build_dir)/client.o
	rm -f $(client_so)
	rm -f $(mex_files)
