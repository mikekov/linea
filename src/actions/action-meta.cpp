#include "action-meta.h"
#include <cstring>

// ActionRegistry& ActionRegistry::instance() {
//     static ActionRegistry instance;
//     return instance;
// }

// void ActionRegistry::registerGroup(const ActionGroup& group) {
//     _groups.push_back(&group);
// }

// std::vector<const ActionGroup*> ActionRegistry::allGroups() const {
//     return _groups;
// }

// const ActionGroup* ActionRegistry::findGroup(const char* id) const {
//     for (const auto* group : _groups) {
//         if (group->id && strcmp(group->id, id) == 0) {
//             return group;
//         }
//     }
//     return nullptr;
// }

// std::vector<ActionRegistry::Entry> ActionRegistry::allActionsBySection() const {
//     std::vector<Entry> result;
//     for (const auto* group : _groups) {
//         for (const auto& def : group->actions) {
//             result.push_back({group, &def.meta});
//         }
//     }
//     return result;
// }
