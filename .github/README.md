# Misc Docs

## Configuration Files

### `plugins.json`

```json
{
  "plugins": {
    "<plugin-name>": {
      "dir": "<path-to-plugin>",
      "target": "<cmake-target-for-plugin>",
      "test": "<cmake-target-for-test>",
      "options": "<cmake-options>",
      "platforms": [
        {
          "key": "<platform-key>"
        },
        {
          "key": "<platform-key>",
          "options": <extra-cmake-options>"
        },
        {
          "key": "<platform-key>",
          "options": <extra-cmake-options>",
          "plugin_tag": "<extra-tag-for-plugin>"
        }
      }
    }
  },
  "platforms": {
    "<platform-key>": {
      "runner": "<github-runner>",
      "docker_tag": "<docker-tag>",
      "asset_tag": "<asset-tag>"
    }
  }
}
```
