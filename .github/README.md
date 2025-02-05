# Misc Docs

## Configuration Files

### `plugins.json`

```json
{
  "plugins": [
    {
      "plugin": "plugin-name",
      "dir": "path-to-plugin",
      "target": "cmake-target",
      "test": "cmake-target-to-test",
      "options": "required-cmake-options",
      "platforms": {
        "some-os_arch": {},
        "another-os_arch": {
          "options": "platform-specific-cmake-options"
        }
      }
  ],
  "platforms": [
    "os_arch": {
      "runner": "github-runner",
      "docker_tag": "wasmedge/wasmedge:tag-here",
      "asset_tag": "os-arch"
    }
  ]
}
```
