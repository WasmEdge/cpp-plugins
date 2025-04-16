module.exports.parse = (config) => {
  let map = new Map();
  for (const platform_key of Object.keys(config.platforms)) {
    map.set(platform_key, []);
  }
  for (const [plugin_key, plugin] of Object.entries(config.plugins)) {
    for (const platform of plugin.platforms) {
      if (!(platform.key in config.platforms))
        continue;
      let copy = { ...plugin, ...config.platforms[platform.key] };
      delete copy.platforms;
      copy.plugin = plugin_key;
      copy.plugin_tag = platform.plugin_tag;
      copy.options = [plugin.options, platform.options].join(" ");
      map.get(platform.key).push(copy);
    }
  }
  return Object.fromEntries(map);
};

if (require.main === module) {
  const { parse } = module.exports;
  const fs = require("fs");
  const s = fs.readFileSync("plugins.json");
  const o = JSON.parse(s);
  console.log(JSON.stringify(parse(o)));
}
