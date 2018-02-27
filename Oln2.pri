HEADERS += \
    ../Oln2/EditUrlDlg.h \
    ../Oln2/HtmlToOutline.h \
    ../Oln2/OutlineDeleg.h \
    ../Oln2/OutlineMdl.h \
    ../Oln2/OutlineStream.h \
    ../Oln2/OutlineToHtml.h \
    ../Oln2/OutlineTree.h \
    ../Oln2/OutlineUdbCtrl.h \
    ../Oln2/OutlineUdbMdl.h \
    ../Oln2/TextToOutline.h \
    ../Oln2/OutlineCtrl.h \
    ../Oln2/LinkSupport.h \
    ../Oln2/RefByItemMdl.h \
    ../Oln2/OutlineUdbStream.h

SOURCES += \
    ../Oln2/EditUrlDlg.cpp \
    ../Oln2/HtmlToOutline.cpp \
    ../Oln2/OutlineDeleg.cpp \
    ../Oln2/OutlineMdl.cpp \
    ../Oln2/OutlineStream.cpp \
    ../Oln2/OutlineToHtml.cpp \
    ../Oln2/OutlineTree.cpp \
    ../Oln2/OutlineUdbCtrl.cpp \
    ../Oln2/OutlineUdbMdl.cpp \
    ../Oln2/TextToOutline.cpp \
    ../Oln2/OutlineCtrl.cpp \
    ../Oln2/LinkSupport.cpp \
    ../Oln2/OutlineItem.cpp \
    ../Oln2/RefByItemMdl.cpp \
	../Oln2/OutlineUdbStream.cpp

HasLua {
SOURCES += \
	../Oln2/OlnLuaBinding.cpp
HEADERS += \
	../Oln2/LuaBinding.h
}
