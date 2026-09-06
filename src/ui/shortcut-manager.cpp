#include "shortcut-manager.h"

#include <QAction>
#include <QFile>
#include <QXmlStreamReader>
#include <iostream>

std::unordered_map<QString, QList<QKeySequence>> ShortcutManager::_defaultShortcuts;
bool ShortcutManager::_defaultsLoaded = false;

ShortcutManager& ShortcutManager::instance() {
    static ShortcutManager instance;
    return instance;
}

ShortcutManager::ShortcutManager()
    : _settings(QSettings::UserScope, "Linea", "LineaDraw") {}

ShortcutManager::~ShortcutManager() = default;

// Linea's XML uses "Cmd" for the platform-primary modifier and "Ctrl" for
// the physical Control modifier. Qt's PortableText swaps those two Qt
// representations on macOS: Ctrl represents Command and Meta represents
// Control. Normalize the XML tokens before passing the string to Qt.
static QList<QKeySequence> parseXmlSequences(QString keys) {
#ifdef Q_OS_MACOS
    // On macOS, Cmd is Command and Ctrl is Control
    keys.replace("Ctrl", "Meta");
    keys.replace("Cmd", "Ctrl");
#else
    // On Windows/Linux, Cmd is a Control
    // but! Ctrl is also Control
    keys.replace("Cmd", "Ctrl");
#endif

    return QKeySequence::listFromString(keys, QKeySequence::PortableText);
}

void ShortcutManager::loadDefaults() {
    if (_defaultsLoaded) return;

    QFile file(":/keys/linea.xml");
    if (!file.open(QIODevice::ReadOnly)) {
        std::cerr << "Failed to open shortcuts resource" << std::endl;
        return;
    }

    QXmlStreamReader xml(&file);
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        if (token == QXmlStreamReader::StartElement && xml.name() == "bind") {
            QString action = xml.attributes().value("action").toString();
            QString keys = xml.attributes().value("keys").toString();
            if (!action.isEmpty() && !keys.isEmpty()) {
                _defaultShortcuts[action] = parseXmlSequences(keys);
            }
        }
    }
    if (xml.hasError()) {
        std::cerr << "Failed to parse shortcuts resource: " << xml.errorString().toStdString() << std::endl;
    }
    _defaultsLoaded = true;
}

void ShortcutManager::load() {
    _settings.beginGroup("UserShortcuts");
    const auto keys = _settings.allKeys();
    for (const auto& key : keys) {
        const auto seqList = QKeySequence::listFromString(
            _settings.value(key).toString(), QKeySequence::PortableText);
        _userShortcuts[key] = seqList;
        for (const auto& seq : seqList) {
            if (!seq.isEmpty()) {
                _shortcutToAction[seq] = key;
            }
        }
    }
    _settings.endGroup();
}

void ShortcutManager::save() {
    _settings.beginGroup("UserShortcuts");
    _settings.remove(""); // Clear existing
    for (const auto& [id, seqs] : _userShortcuts) {
        _settings.setValue(id, QKeySequence::listToString(seqs, QKeySequence::PortableText));
    }
    _settings.endGroup();
    _settings.sync();
}

void ShortcutManager::applyTo(QAction* action, const QString& actionId) {
    QList<QKeySequence> seqs = shortcuts(actionId);
    if (!seqs.isEmpty()) {
        action->setShortcuts(seqs);
    }
}

QList<QKeySequence> ShortcutManager::shortcuts(const QString& actionId) const {
    auto it = _userShortcuts.find(actionId);
    if (it != _userShortcuts.end()) {
        return it->second;
    }
    return defaultShortcuts(actionId);
}

QKeySequence ShortcutManager::shortcut(const QString& actionId) const {
    QList<QKeySequence> seqs = shortcuts(actionId);
    return seqs.isEmpty() ? QKeySequence() : seqs.first();
}

QList<QKeySequence> ShortcutManager::userShortcuts(const QString& actionId) const {
    auto it = _userShortcuts.find(actionId);
    return it != _userShortcuts.end() ? it->second : QList<QKeySequence>();
}

QList<QKeySequence> ShortcutManager::defaultShortcuts(const QString& actionId) {
    loadDefaults();
    auto it = _defaultShortcuts.find(actionId);
    return it != _defaultShortcuts.end() ? it->second : QList<QKeySequence>();
}

void ShortcutManager::setShortcut(const QString& actionId, const QList<QKeySequence>& seqs) {
    // Remove old reverse mappings if exists
    auto oldSeqs = userShortcuts(actionId);
    for (const auto& oldSeq : oldSeqs) {
        _shortcutToAction.erase(oldSeq);
    }

    _userShortcuts[actionId] = seqs;
    for (const auto& seq : seqs) {
        if (!seq.isEmpty()) {
            _shortcutToAction[seq] = actionId;
        }
    }

    // Emit signal with first shortcut for backward compatibility
    shortcutChanged(actionId, seqs.isEmpty() ? QKeySequence() : seqs.first());
}

void ShortcutManager::resetShortcut(const QString& actionId) {
    auto it = _userShortcuts.find(actionId);
    if (it != _userShortcuts.end()) {
        // Remove all reverse mappings
        for (const auto& seq : it->second) {
            _shortcutToAction.erase(seq);
        }
        _userShortcuts.erase(it);
        QList<QKeySequence> defaults = defaultShortcuts(actionId);
        shortcutChanged(actionId, defaults.isEmpty() ? QKeySequence() : defaults.first());
    }
}

void ShortcutManager::resetAll() {
    _userShortcuts.clear();
    _shortcutToAction.clear();
    shortcutChanged("", QKeySequence()); // Mass reset signal
}

bool ShortcutManager::isUserSet(const QString& actionId) const {
    return _userShortcuts.find(actionId) != _userShortcuts.end();
}

bool ShortcutManager::invokeAction(const QKeySequence& seq) {
    auto it = _shortcutToAction.find(seq);
    if (it != _shortcutToAction.end()) {
        // TODO: Find the action and trigger it
        std::cerr << "ShortcutManager::invokeAction: not implemented" << std::endl;
        return true;
    }
    return false;
}
