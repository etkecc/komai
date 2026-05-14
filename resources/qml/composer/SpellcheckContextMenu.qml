// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import cc.etke.komai 1.0

// Right-click context menu for a TextArea/TextEdit that has a SpellChecker
// attached. Builds per-dictionary spelling-suggestion submenus when the click
// landed on a misspelled word, plus the standard edit actions (Undo, Redo,
// Cut, Copy, Paste, Select All). Used by the message composer and the
// room-topic editor.
//
// Usage:
//
//   SpellcheckContextMenu {
//       id: spellcheckCtx
//       target: myTextArea
//       spellChecker: mySpellChecker
//   }
//   MouseArea {
//       anchors.fill: myTextArea
//       acceptedButtons: Qt.RightButton
//       cursorShape: Qt.IBeamCursor
//       onPressed: mouse => { mouse.accepted = true; }
//       onClicked: mouse => spellcheckCtx.show(Qt.point(mouse.x, mouse.y))
//   }
QtObject {
    id: root

    // The TextArea/TextEdit. Must support positionAt(x,y), cut/copy/paste/
    // selectAll/undo/redo methods, canPaste/canUndo/canRedo/selectedText/
    // length/readOnly properties.
    required property var target

    // The SpellChecker bound to target.textDocument. Provides
    // misspelledWordAround(pos) and replaceRange(start, length, replacement).
    required property var spellChecker

    function show(point) {
        const t = root.target;
        const sc = root.spellChecker;
        const pos = t.positionAt(point.x, point.y);
        const info = sc.misspelledWordAround(pos);
        const menu = _menuComponent.createObject(t);
        if (!menu)
            return;

        if (info.found) {
            const groups = SpellCheckEngine.suggestionsFor(info.word);
            for (let g = 0; g < groups.length; ++g) {
                const group = groups[g];
                //: Submenu header in the right-click spelling-suggestions
                //: menu. %1 is the dictionary's display name
                //: ("English / United States", "Bulgarian / Bulgaria",
                //: or just "Esperanto" for locale codes without a
                //: territory); %2 is the number of suggestions in this
                //: group (an integer).
                const sub = _submenuComponent.createObject(t, {
                    title: qsTr("Spellcheck (%1): [%2]").arg(group.language).arg(group.suggestions.length)
                });
                for (let s = 0; s < group.suggestions.length; ++s) {
                    const w = group.suggestions[s];
                    sub.addItem(_mkItem(w, true, function () {
                        sc.replaceRange(info.start, info.length, w);
                    }, ""));
                }
                menu.addMenu(sub);
                MenuSizing.sizeMenuToContents(sub);
            }
            if (groups.length === 0)
                menu.addItem(_mkItem(qsTr("No spelling suggestions"), false, null, ""));
            menu.addItem(_separatorComponent.createObject(t));
            menu.addItem(_mkItem(qsTr("Add to dictionary"), true, function () {
                SpellCheckEngine.addToDictionary(info.word);
            }, ""));
            menu.addItem(_separatorComponent.createObject(t));
        }

        // Undo/Redo when the target supports it (every TextArea does; left
        // optional so unusual targets without these still work).
        if (t.canUndo !== undefined) {
            menu.addItem(_mkItem(qsTr("Undo"), t.canUndo && !t.readOnly,
                function () { t.undo(); }, "qrc:/icons/icons/ui/undo.svg"));
            menu.addItem(_mkItem(qsTr("Redo"), t.canRedo && !t.readOnly,
                function () { t.redo(); }, "qrc:/icons/icons/ui/redo.svg"));
            menu.addItem(_separatorComponent.createObject(t));
        }
        menu.addItem(_mkItem(qsTr("Cut"),
            t.selectedText.length > 0 && !t.readOnly,
            function () { t.cut(); }, "qrc:/icons/icons/ui/cut.svg"));
        menu.addItem(_mkItem(qsTr("Copy"),
            t.selectedText.length > 0,
            function () { t.copy(); }, "qrc:/icons/icons/ui/copy.svg"));
        menu.addItem(_mkItem(qsTr("Paste"),
            t.canPaste,
            function () { t.paste(); }, "qrc:/icons/icons/ui/paste.svg"));
        menu.addItem(_separatorComponent.createObject(t));
        menu.addItem(_mkItem(qsTr("Select All"),
            t.length > 0,
            function () { t.selectAll(); }, "qrc:/icons/icons/ui/select-all-on.svg"));

        MenuSizing.sizeMenuToContents(menu);
        menu.closed.connect(function () { menu.destroy(); });
        menu.popup(point);
    }

    // Items/separators are parented to `target` (which is in the graphics
    // scene, so there's no "not placed in the graphics scene" warning) and
    // then addItem/addMenu reparents them into the menu — which therefore
    // owns them and frees them when it's destroyed.
    //
    // iconSource is a qrc path (e.g. "qrc:/icons/icons/ui/cut.svg") rather
    // than a freedesktop theme name, so the rendered glyph is identical on
    // every install regardless of which system icon theme is active.
    function _mkItem(label, isEnabled, action, iconSource) {
        const opts = { text: label, enabled: isEnabled };
        if (iconSource)
            opts["icon.source"] = iconSource;
        const item = _itemComponent.createObject(root.target, opts);
        if (action)
            item.triggered.connect(action);
        return item;
    }

    // `cascade` keeps sub-menus opening to the side rather than
    // overlaying/replacing the parent; the default popupType (an in-window
    // item popup) makes that work reliably.
    property Component _menuComponent: Component { Menu { cascade: true } }
    property Component _submenuComponent: Component { Menu { cascade: true } }
    property Component _itemComponent: Component { MenuItem {} }
    property Component _separatorComponent: Component { MenuSeparator {} }
}
