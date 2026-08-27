import { getOhosExportModulesFactories } from './OhosExportModules';
import { getLocalStorageTsInterface } from './LocalStorageTsModule';

export default class QtUtils {
  static getModulesFactoriesMapForQt(): object {
    const factories: object = getOhosExportModulesFactories();
    (factories as Record<string, object>)['LocalStorage'] = getLocalStorageTsInterface;
    return factories;
  }
}
