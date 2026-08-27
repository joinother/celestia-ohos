import AbilityStage from '@ohos.app.ability.AbilityStage';
import QAbility from '../qability/QAbility';
import QtUtils from '../qability/QtUtils';
import Want from '@ohos.app.ability.Want';
import common from '@ohos.app.ability.common';
import hilog from '@ohos.hilog';
import qpa from 'libqohos.so';
import { APP_LIBRARY_NAME, LOG_DOMAIN, LOG_TAG } from '../common/QtAppConstants';
import { prepareCelestiaResources } from '../qability/CelestiaResourceBootstrap';

export default class QAbilityStage extends AbilityStage {

  private static appArgs: Array<string> = [];

  private static setupQtApplicationCalled: boolean = false;
  private static setupQtApplicationPromise: Promise<void> | undefined = undefined;
  private static stageCreateForwarded: boolean = false;

  private static async initQtAppContextImpl(appContext: common.ApplicationContext, abilityClassName: string): Promise<void> {
    if (!QAbilityStage.setupQtApplicationCalled) {
      hilog.info(LOG_DOMAIN, LOG_TAG, 'QAbilityStage::initQtAppContextImpl: init');
      const dataDir = await prepareCelestiaResources(appContext);
      QAbilityStage.appArgs = ['--dir', dataDir];
      qpa.setupQtApplication({
        appContext: appContext,
        modulesFactories: QtUtils.getModulesFactoriesMapForQt(),
        appName: APP_LIBRARY_NAME,
        appArgs: QAbilityStage.appArgs,
        abilityClassName: abilityClassName,
      });
      QAbilityStage.setupQtApplicationCalled = true;
      QAbilityStage.forwardStageCreateIfNeeded();
    } else {
      hilog.info(LOG_DOMAIN, LOG_TAG, 'QAbilityStage::initQtAppContextImpl: already initialized');
    }
  }

  public static initQtAppContextIfNeeded(appContext: common.ApplicationContext): Promise<void> {
    if (QAbilityStage.setupQtApplicationPromise === undefined) {
      QAbilityStage.setupQtApplicationPromise = QAbilityStage.initQtAppContextImpl(appContext, QAbility.name)
        .catch(() => {
          QAbilityStage.setupQtApplicationPromise = undefined;
          throw new Error('Qt application context initialization failed');
        });
    }
    return QAbilityStage.setupQtApplicationPromise;
  }

  private static forwardStageCreateIfNeeded(): void {
    if (QAbilityStage.stageCreateForwarded || QAbilityStage.setupQtApplicationPromise === undefined) {
      return;
    }
    QAbilityStage.stageCreateForwarded = true;
  }

  onCreate(): void {
    hilog.info(LOG_DOMAIN, LOG_TAG, 'QAbilityStage::onCreate()');
    QAbilityStage.forwardStageCreateIfNeeded();
  }

  onNewProcessRequest(want: Want): string {
    hilog.info(LOG_DOMAIN, LOG_TAG, 'QAbilityStage::onNewProcessRequest');
    return '';
  }

  onAcceptWant(want: Want): string {
    hilog.info(LOG_DOMAIN, LOG_TAG, 'QAbilityStage::onAcceptWant');
    return '';
  }

  onDestroy() {
    hilog.info(LOG_DOMAIN, LOG_TAG, 'QAbilityStage::onDestroy()');
  }
}
