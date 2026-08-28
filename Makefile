CC ?= gcc
WINDRES ?= windres
CFLAGS ?= -std=c11 -O2 -s -fno-ident -Wall -Wextra -pedantic -Wno-cast-function-type -Wno-discarded-qualifiers -Wno-use-after-free -Wno-format-truncation -Wno-overlength-strings -I./src
STUDIO_CFLAGS = -std=c11 -O2 -s -fno-ident -Wall -Wextra -pedantic -Wno-cast-function-type -Wno-discarded-qualifiers -Wno-use-after-free -Wno-format-truncation -Wno-overlength-strings -I./src -I./studio

WIN_LIBS = -lcomdlg32 -lcomctl32 -lgdi32 -luser32 -lkernel32 -lole32 -loleaut32 -luuid -ladvapi32 -lwindowscodecs -lmsimg32 -lwinmm -lpropsys -lshell32 -lwinhttp -lcrypt32


COMMON_SRCS = \
	src/error.c \
	src/console.c \
	src/token.c \
	src/lexer.c \
	src/ast.c \
	src/parser.c \
	src/fileio.c \
	src/media.c \
	src/module.c \
	src/semantic.c \
	src/frontend.c \
	src/ui.c \
	src/runtime.c \
	src/license_guard.c \
	src/packager.c \
	src/compiler.c \
	src/ide_highlight.c


FRONTEND_TEST_SRCS = \
	tests/test_frontend.c \
	src/error.c \
	src/token.c \
	src/lexer.c \
	src/ast.c \
	src/parser.c \
	src/fileio.c \
	src/module.c \
	src/semantic.c \
	src/frontend.c

CLI_SRCS = src/main.c $(COMMON_SRCS)
RUNNER_SRCS = src/launcher.c $(COMMON_SRCS)
STUDIO_SRCS = \
	studio/main.c \
	studio/app.c \
	studio/bridge.c \
	studio/debug_log.c \
	studio/json.c \
	studio/outline.c \
	studio/project.c \
	studio/runner.c \
	studio/settings.c \
	studio/templates.c \
	studio/util.c \
	studio/webview2_host.c \
	src/license_guard.c \
	src/error.c \
	src/fileio.c
WEBVIEW2_DEBUG_SRCS = \
	studio/webview2_fixed_runtime_debug.c
CLI_RES = erire_cli_res.o
RUNNER_RES = erire_runner_res.o
STUDIO_RES = erire_studio_res.o

.PHONY: all clean test refresh-brand-icon firststand-builder firststand-erire-installer

all: erire.exe ErireRunner.exe ErireStudio.exe

BRAND_ICON_PNG = assets/brand/erire-logo.png
BRAND_ICON_ICO = assets/brand/erire-logo.ico

refresh-brand-icon: $(BRAND_ICON_PNG) tools/png_to_ico.py
	py -3 tools/png_to_ico.py $(BRAND_ICON_PNG) $(BRAND_ICON_ICO)

$(CLI_RES): resources/erire_cli.rc $(BRAND_ICON_ICO)
	cd resources && $(WINDRES) -I.. -i erire_cli.rc -O coff -o ../$@

$(RUNNER_RES): resources/erire_runner.rc $(BRAND_ICON_ICO)
	cd resources && $(WINDRES) -I.. -i erire_runner.rc -O coff -o ../$@

$(STUDIO_RES): resources/erire_studio.rc $(BRAND_ICON_ICO)
	cd resources && $(WINDRES) -I.. -i erire_studio.rc -O coff -o ../$@

erire.exe: $(CLI_SRCS) $(CLI_RES)
	$(CC) $(CFLAGS) -o $@ $(CLI_SRCS) $(CLI_RES) $(WIN_LIBS)

ErireRunner.exe: $(RUNNER_SRCS) $(RUNNER_RES)
	$(CC) $(CFLAGS) -mwindows -o $@ $(RUNNER_SRCS) $(RUNNER_RES) $(WIN_LIBS)

ErireStudio.exe: $(STUDIO_SRCS) $(STUDIO_RES)
	$(CC) $(STUDIO_CFLAGS) -mwindows -o $@ $(STUDIO_SRCS) $(STUDIO_RES) $(WIN_LIBS) -lshell32

WebView2FixedRuntimeDebug.exe: $(WEBVIEW2_DEBUG_SRCS)
	$(CC) $(STUDIO_CFLAGS) -mwindows -o $@ $(WEBVIEW2_DEBUG_SRCS) $(WIN_LIBS)

frontend_tests.exe: $(FRONTEND_TEST_SRCS)
	$(CC) $(CFLAGS) -o $@ $(FRONTEND_TEST_SRCS)

test: frontend_tests.exe
	.\frontend_tests.exe

firststand-builder:
	powershell -ExecutionPolicy Bypass -File tools\firststand_installer\build.ps1

firststand-erire-installer: firststand-builder
	cd tools\firststand_installer && bin\FirstStand-Installer.exe examples\erire.installer.ini

clean:
	del /Q erire.exe ErireRunner.exe ErireStudio.exe frontend_tests.exe $(CLI_RES) $(RUNNER_RES) $(STUDIO_RES) 2>NUL || exit 0
