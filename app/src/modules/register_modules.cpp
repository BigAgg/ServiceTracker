#include "framework/module.h"

#include "modules/changes/changes_module.h"
#include "modules/checklist/checklist_module.h"
#include "modules/configs/configs_module.h"
#include "modules/devices/devices_module.h"
#include "modules/extern/extern_module.h"
#include "modules/general/general_module.h"
#include "modules/history/history_module.h"
#include "modules/orders/orders_module.h"
#include "modules/service/service_module.h"
#include "modules/users/users_module.h"

namespace st::app {

/// All modules of the program. The order is the initialisation order:
/// modules that register expressions/actions come before the ones using them.
/// To add a module: create modules/<name>/, implement Module, add one line here
/// (see docs/module.md).
void registerModules(ModuleRegistry &registry) {
  registry.add<GeneralModule>();
  registry.add<DevicesModule>();
  registry.add<ServiceModule>();
  registry.add<ChecklistModule>();
  registry.add<ExternModule>();
  registry.add<HistoryModule>();
  registry.add<OrdersModule>();
  registry.add<ConfigsModule>();
  registry.add<UsersModule>();
  registry.add<ChangesModule>();
}

} // namespace st::app
