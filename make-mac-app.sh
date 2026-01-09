#!/bin/sh

# clean everything and recompile
make clean
rm -rf ./VirtualGlass.app/
rm -f ./VirtualGlass.dmg

# recreate makefiles for release (Qt 6 uses macx-clang spec)
qmake -spec macx-clang -config release virtualglass.pro
make -j4
bash make-version.sh

# pull the necessary frameworks into the bundle
macdeployqt ./VirtualGlass.app/

# sign the app for modern macOS (must be after macdeployqt)
codesign --force --deep --sign - ./VirtualGlass.app/

# create DMG
hdiutil create -volname VirtualGlass -srcfolder ./VirtualGlass.app -ov -format UDZO ./VirtualGlass.dmg

# reset compilation setup to development settings
qmake -spec macx-clang -config debug virtualglass.pro

