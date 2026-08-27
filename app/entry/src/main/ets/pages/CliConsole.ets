import { fileIo as fs } from '@kit.CoreFileKit';
import { common } from '@kit.AbilityKit';

/**
 * CliConsole — 应用内命令行控制台（Celestia CLI）
 *
 * 底部可折叠的命令输入条。命令写入 <filesDir>/celestia/cli_commands.txt，
 * Qt 侧 (CelestiaAppWindow::processCliCommands) 轮询读取并执行。
 *
 * 支持命令：goto/select/center/follow/syncorbit/home/time/render/label/...
 */
@Component
export struct CliConsole {
  @State expanded: boolean = false;
  @State cmdText: string = '';
  private cmdHistory: string[] = [];
  private histIdx: number = -1;

  private getCmdFilePath(): string {
    const ctx = getContext(this) as common.UIAbilityContext;
    return ctx.filesDir + '/celestia/cli_commands.txt';
  }

  private sendCommand(): void {
    const cmd = this.cmdText.trim();
    if (cmd.length === 0) {
      return;
    }
    try {
      fs.appendTextSync(this.getCmdFilePath(), cmd + '\n');
    } catch (e) {
      console.error(`CliConsole::append failed: ${JSON.stringify(e)}`);
    }
    this.cmdHistory.push(cmd);
    if (this.cmdHistory.length > 50) {
      this.cmdHistory.shift();
    }
    this.histIdx = -1;
    this.cmdText = '';
  }

  private onKeyEnter(): void {
    this.sendCommand();
  }

  private onKeyUp(): void {
    if (this.cmdHistory.length === 0) {
      return;
    }
    this.histIdx = this.histIdx < 0 ? this.cmdHistory.length - 1 : Math.max(0, this.histIdx - 1);
    this.cmdText = this.cmdHistory[this.histIdx];
  }

  private onKeyDown(): void {
    if (this.cmdHistory.length === 0) {
      return;
    }
    this.histIdx = this.histIdx < 0 ? this.cmdHistory.length - 1 : Math.min(this.cmdHistory.length - 1, this.histIdx + 1);
    this.cmdText = this.cmdHistory[this.histIdx];
  }

  build() {
    Column() {
      // 折叠开关
      Row() {
        Text(this.expanded ? '▾ CLI 命令' : '▸ CLI 命令')
          .fontSize(13)
          .fontColor('#FF8AB4F8')
          .onClick(() => {
            this.expanded = !this.expanded;
          })
      }
      .width('100%')
      .padding({ left: 12, top: 4, bottom: 4 })
      .backgroundColor('#CC1E1E2E')

      if (this.expanded) {
        Row({ space: 6 }) {
          TextInput({ placeholder: '输入命令，如 goto mars / time scale 10000x / render orbits', text: this.cmdText })
            .layoutWeight(1)
            .height(34)
            .fontSize(13)
            .backgroundColor('#FF2A2A3C')
            .fontColor('#FFFFFFFF')
            .placeholderColor('#FF888899')
            .onChange((v: string) => {
              this.cmdText = v;
            })
            .onSubmit(() => {
              this.onKeyEnter();
            })
            .onKeyEvent((event: KeyEvent) => {
              if (event.type === KeyType.Down) {
                if (event.keyCode === KeyCode.KEYCODE_DPAD_UP) {
                  this.onKeyUp();
                } else if (event.keyCode === KeyCode.KEYCODE_DPAD_DOWN) {
                  this.onKeyDown();
                }
              }
            })

          Button('发送')
            .height(34)
            .fontSize(13)
            .backgroundColor('#FF3B82F6')
            .onClick(() => {
              this.sendCommand();
            })
        }
        .width('100%')
        .padding({ left: 12, right: 12, top: 2, bottom: 2 })

        Text('支持：goto/select/center/follow/syncorbit/home · time scale <倍率> · render/label <标记> · setsurface · location <纬度> <经度> · script <路径> · help')
          .fontSize(11)
          .fontColor('#FF8888AA')
          .width('100%')
          .padding({ left: 12, right: 12, bottom: 4 })
      }
    }
    .width('100%')
    .alignItems(HorizontalAlign.Start)
  }
}
