#ifndef LINEA_SHORTCUT_MANAGER_H
#define LINEA_SHORTCUT_MANAGER_H

#include <QKeySequence>
#include <QList>
#include <QSettings>
#include <QString>
#include <unordered_map>
#include <sigc++/signal.h>

// Hash function for QKeySequence to use in unordered_map
namespace std {
template <>
struct hash<QKeySequence> {
    size_t operator()(const QKeySequence& seq) const { return qHash(seq); }
};
} // namespace std

class QAction;

class ShortcutManager {
public:
    static ShortcutManager& instance();

    void load(); // Load from QSettings
    void save(); // Save to QSettings

    // Apply saved shortcut to QAction by ID
    void applyTo(QAction* action, const QString& actionId);

    // Get current shortcuts (user-set or default)
    QList<QKeySequence> shortcuts(const QString& actionId) const;

    // Get primary shortcut (first in list, for backward compatibility)
    QKeySequence shortcut(const QString& actionId) const;

    // Get just the user-set shortcuts (empty if not customized)
    QList<QKeySequence> userShortcuts(const QString& actionId) const;

    // Get default shortcuts for action
    static QList<QKeySequence> defaultShortcuts(const QString& actionId);

    // Set user shortcuts
    void setShortcut(const QString& actionId, const QList<QKeySequence>& seqs);

    // Restore defaults
    void resetShortcut(const QString& actionId);
    void resetAll();

    // Check if user has customized
    bool isUserSet(const QString& actionId) const;

    // Invoke action by key (for direct key event handling)
    bool invokeAction(const QKeySequence& seq);

    // sigc++ signal for shortcut changes
    sigc::signal<void(const QString&, const QKeySequence&)> shortcutChanged;

    ShortcutManager(const ShortcutManager&) = delete;
    ShortcutManager& operator = (const ShortcutManager&) = delete;
private:
    ShortcutManager();
    ~ShortcutManager();

    static void loadDefaults();

    QSettings _settings;
    std::unordered_map<QString, QList<QKeySequence>> _userShortcuts;
    std::unordered_map<QKeySequence, QString> _shortcutToAction; // Reverse lookup

    static std::unordered_map<QString, QList<QKeySequence>> _defaultShortcuts;
    static bool _defaultsLoaded;
};

#endif
