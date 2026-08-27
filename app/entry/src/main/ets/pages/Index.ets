export interface WindowNativeNodeCreateInfo {
  xComponentId: string
  onAppear: () => void
  onAttach: () => void
  onDisAppear: () => void
}

@Entry
@Component
struct Index {
  @State createInfo: WindowNativeNodeCreateInfo | undefined = undefined;

  aboutToAppear(): void {
    this.loadCreateInfo();
  }

  loadCreateInfo(): void {
    const uiContext = this.getUIContext();
    const storage = uiContext.getSharedLocalStorage();
    if (storage !== undefined) {
      this.createInfo = storage.get<WindowNativeNodeCreateInfo>('createInfo');
    }
    if (this.createInfo === undefined) {
      setTimeout(() => {
        this.loadCreateInfo();
      }, 50);
    }
  }

  build() {
    Stack() {
      if (this.createInfo !== undefined) {
        XComponent({
          type: XComponentType.NODE,
          id: this.createInfo.xComponentId,
          libraryname: 'qohos'
        })
          .onAttach(this.createInfo.onAttach)
          .onAppear(this.createInfo.onAppear)
          .onDisAppear(this.createInfo.onDisAppear)
          .width('100%')
          .height('100%')
      }
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#000000')
  }
}
