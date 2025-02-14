TARGET := iphone:clang:16.5:15.0
INSTALL_TARGET_PROCESSES = TimeBomb2

ARCHS = arm64

include $(THEOS)/makefiles/common.mk

APPLICATION_NAME = TimeBomb2

TimeBomb2_FILES = $(wildcard *.c *.m litehook/src/*.c)
TimeBomb2_FRAMEWORKS = UIKit CoreGraphics
TimeBomb2_PRIVATE_FRAMEWORKS = Preferences
TimeBomb2_CFLAGS = -fobjc-arc -Ilitehook/src -Ilitehook/external/include
TimeBomb2_CODESIGN_FLAGS = -Sentitlements.plist

include $(THEOS_MAKE_PATH)/application.mk
SUBPROJECTS += spinlock_child
include $(THEOS_MAKE_PATH)/aggregate.mk
