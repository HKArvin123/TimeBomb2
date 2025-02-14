TARGET := iphone:clang:16.5:15.0

ARCHS = arm64

include $(THEOS)/makefiles/common.mk

TOOL_NAME = spinlock_child

spinlock_child_FILES = $(wildcard *.c ../litehook/src/*.c)
spinlock_child_CFLAGS = -fobjc-arc -Wno-deprecated-declarations -I../litehook/src -I../litehook/external/include
spinlock_child_CODESIGN_FLAGS = -Sentitlements.plist
spinlock_child_INSTALL_PATH = /Applications/TimeBomb2.app
spinlock_child_FRAMEWORKS = Foundation

include $(THEOS_MAKE_PATH)/tool.mk
