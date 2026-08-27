import { common } from '@kit.AbilityKit';
import { fileIo as fs } from '@kit.CoreFileKit';
import hilog from '@ohos.hilog';
import { LOG_DOMAIN, LOG_TAG } from '../common/QtAppConstants';

const RAW_ROOT = 'celestia';
const MARKER = '.celestia_extracted';
/* 关键文件指纹：缺失则强制重新解压（用于增量更新 rawfile，如新增 .mo） */
const KEY_FILE = 'locale/zh_CN/LC_MESSAGES/celestia.mo';

function exists(path: string): boolean {
  try {
    fs.statSync(path);
    return true;
  } catch (_err) {
    return false;
  }
}

function ensureDir(path: string): void {
  if (exists(path)) {
    return;
  }
  try {
    fs.mkdirSync(path, true);
  } catch (err) {
    hilog.error(LOG_DOMAIN, LOG_TAG, `Failed to create directory ${path}: ${JSON.stringify(err)}`);
    throw new Error(`Failed to create directory: ${path}`);
  }
}

async function writeRawFile(appContext: common.ApplicationContext, rawPath: string, outputPath: string): Promise<void> {
  ensureDir(outputPath.substring(0, outputPath.lastIndexOf('/')));
  let file: fs.File | undefined = undefined;
  try {
    const content = await appContext.resourceManager.getRawFileContent(rawPath);
    file = await fs.open(outputPath, fs.OpenMode.CREATE | fs.OpenMode.TRUNC | fs.OpenMode.WRITE_ONLY);
    const data = content.buffer.slice(content.byteOffset, content.byteOffset + content.byteLength) as ArrayBuffer;
    await fs.write(file.fd, data);
  } catch (err) {
    hilog.error(LOG_DOMAIN, LOG_TAG, `Failed to write raw resource ${rawPath}: ${JSON.stringify(err)}`);
    throw new Error(`Failed to write raw resource: ${rawPath}`);
  } finally {
    if (file !== undefined) {
      try {
        await fs.close(file);
      } catch (err) {
        hilog.error(LOG_DOMAIN, LOG_TAG, `Failed to close resource file ${outputPath}: ${JSON.stringify(err)}`);
      }
    }
  }
}

async function extractRawTree(appContext: common.ApplicationContext, rawDir: string, outputDir: string): Promise<void> {
  ensureDir(outputDir);
  let entries: Array<string>;
  try {
    entries = await appContext.resourceManager.getRawFileList(rawDir);
  } catch (err) {
    hilog.error(LOG_DOMAIN, LOG_TAG, `Failed to list raw resource directory ${rawDir}: ${JSON.stringify(err)}`);
    throw new Error(`Failed to list raw resource directory: ${rawDir}`);
  }
  for (const entry of entries) {
    const rawPath = rawDir.length > 0 ? `${rawDir}/${entry}` : entry;
    const outputPath = `${outputDir}/${entry}`;
    try {
      await appContext.resourceManager.getRawFileList(rawPath);
      await extractRawTree(appContext, rawPath, outputPath);
    } catch (_err) {
      await writeRawFile(appContext, rawPath, outputPath);
    }
  }
}

export async function prepareCelestiaResources(appContext: common.ApplicationContext): Promise<string> {
  const destDir = `${appContext.filesDir}/celestia`;
  const markerPath = `${destDir}/${MARKER}`;
  const keyFilePath = `${destDir}/${KEY_FILE}`;
  hilog.info(LOG_DOMAIN, LOG_TAG, `prepareCelestiaResources: filesDir=${appContext.filesDir} markerExists=${exists(markerPath)} keyExists=${exists(keyFilePath)}`);
  if (exists(markerPath) && exists(keyFilePath)) {
    hilog.info(LOG_DOMAIN, LOG_TAG, `Celestia data already extracted at ${destDir}`);
    return destDir;
  }
  hilog.info(LOG_DOMAIN, LOG_TAG, 'Celestia data extraction started');
  const started = Date.now();
  try {
    await extractRawTree(appContext, RAW_ROOT, destDir);
    let marker: fs.File | undefined = undefined;
    try {
      marker = await fs.open(markerPath, fs.OpenMode.CREATE | fs.OpenMode.TRUNC | fs.OpenMode.WRITE_ONLY);
    } finally {
      if (marker !== undefined) {
        await fs.close(marker);
      }
    }
    hilog.info(LOG_DOMAIN, LOG_TAG, `Celestia data extraction finished in ${Date.now() - started} ms`);
  } catch (err) {
    hilog.error(LOG_DOMAIN, LOG_TAG, `Celestia data extraction failed: ${JSON.stringify(err)}`);
  }
  return destDir;
}
