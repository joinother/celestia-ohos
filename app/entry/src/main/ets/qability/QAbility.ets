import AbilityConstant from '@ohos.app.ability.AbilityConstant';
import QAbilityStage from '../qabilitystage/QAbilityStage';
import UIAbility from '@ohos.app.ability.UIAbility';
import Want from '@ohos.app.ability.Want';
import Window from '@ohos.window';
import hilog from '@ohos.hilog';
import qpa from 'libqohos.so';
import { LOG_DOMAIN, LOG_TAG } from '../common/QtAppConstants';

export default class QAbility extends UIAbility {

  private pendingWant: Want | null = null;
  private pendingLaunchParam: AbilityConstant.LaunchParam | null = null;
  private qtAbilityCreated: boolean = false;
  private qtInitialized: boolean = false;

  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam) {
    hilog.info(LOG_DOMAIN, LOG_TAG, 'QAbility::onCreate()');
    this.pendingWant = want;
    this.pendingLaunchParam = launchParam;
  }

  onDestroy(): void | Promise<void> {
    hilog.info(LOG_DOMAIN, LOG_TAG, 'QAbility::onDestroy()');
    if (this.qtAbilityCreated) {
      return qpa.handleAbilityOnDestroy(this);
    }
  }

  onPrepareToTerminate(): boolean {
    hilog.info(LOG_DOMAIN, LOG_TAG, 'QAbility::onPrepareToTerminate()');
    return this.qtAbilityCreated ? qpa.handleAbilityOnPrepareToTerminate(this) : false;
  }

  onPrepareToTerminateAsync(): Promise<boolean> {
    hilog.info(LOG_DOMAIN, LOG_TAG, 'QAbility::onPrepareToTerminateAsync()');
    return this.qtAbilityCreated ? qpa.handleAbilityOnPrepareToTerminateAsync(this) : Promise.resolve(false);
  }

  async onWindowStageCreate(windowStage: Window.WindowStage) {
    hilog.info(LOG_DOMAIN, LOG_TAG, 'QAbility::onWindowStageCreate()');
    try {
      await QAbilityStage.initQtAppContextIfNeeded(this.context.getApplicationContext());
      if (this.pendingWant !== null && this.pendingLaunchParam !== null) {
        qpa.handleAbilityOnCreate(this, this.pendingWant, this.pendingLaunchParam);
        this.qtAbilityCreated = true;
      }
      qpa.handleAbilityOnWindowStageCreate(this, windowStage);
      this.qtInitialized = true;
      hilog.info(LOG_DOMAIN, LOG_TAG, 'QAbility::Qt initialized and window stage handed to Qt');
      // 鸿蒙电脑端：延迟等 Qt 创建主窗口后最大化，铺满屏幕
      setTimeout(() => {
        this.maximizeWindow(windowStage).catch((err: Error) => {
          hilog.warn(LOG_DOMAIN, LOG_TAG, `QAbility::maximizeWindow failed: ${JSON.stringify(err)}`);
        });
      }, 1500);
    } catch (err) {
      hilog.error(LOG_DOMAIN, LOG_TAG, `QAbility::Qt startup failed: ${JSON.stringify(err)}`);
    }
  }

  private async maximizeWindow(windowStage: Window.WindowStage): Promise<void> {
    try {
      const mainWindow: Window.Window | undefined = await windowStage.getMainWindow();
      if (mainWindow !== undefined) {
        await mainWindow.maximize();
        hilog.info(LOG_DOMAIN, LOG_TAG, 'QAbility::window maximized');
      }
    } catch (err) {
      hilog.warn(LOG_DOMAIN, LOG_TAG, `QAbility::getMainWindow/maximize failed: ${JSON.stringify(err)}`);
    }
  }

  onWindowStageRestore(windowStage: Window.WindowStage): void {
    hilog.info(LOG_DOMAIN, LOG_TAG, 'QAbility::onWindowStageRestore()');
    if (this.qtInitialized) {
      qpa.handleAbilityOnWindowStageRestore(this, windowStage);
    }
  }

  onNewWant(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    hilog.info(LOG_DOMAIN, LOG_TAG, 'QAbility::onNewWant()');
    this.pendingWant = want;
    this.pendingLaunchParam = launchParam;
    if (this.qtAbilityCreated) {
      qpa.handleAbilityOnNewWant(this, want, launchParam);
    }
  }

  onForeground() {
    hilog.info(LOG_DOMAIN, LOG_TAG, 'QAbility::onForeground()');
    if (!this.qtInitialized) {
      return;
    }
    qpa.handleAbilityOnForeground(this);
  }

  onBackground() {
    hilog.info(LOG_DOMAIN, LOG_TAG, 'QAbility::onBackground()');
    if (!this.qtInitialized) {
      return;
    }
    qpa.handleAbilityOnBackground(this);
  }

  onWindowStageDestroy() {
    hilog.info(LOG_DOMAIN, LOG_TAG, 'QAbility::onWindowStageDestroy()');
    if (this.qtAbilityCreated) {
      qpa.handleAbilityOnWindowStageDestroy(this);
    }
  }
}
