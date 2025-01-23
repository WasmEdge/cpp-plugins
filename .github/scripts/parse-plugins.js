module.exports.parse = (config) => {
  let map = new Map();
  for (const [key, platform] of Object.entries(config.platforms)) {
    map.set(key, config.plugins
      .map((plugin) => {
        let specific = plugin.platforms[key];
        if (undefined == specific)
          return undefined;
        let copy = { ...plugin, ...platform };
        delete copy.platforms;
        copy.options = [plugin.options, specific.options].join(" ");
        return copy;
      })
      .filter((plugin) => undefined != plugin));
  }
  return Object.fromEntries(map);
};

if (require.main === module) {
  const { parse } = module.exports;
  const fs = require("fs");
  const s = fs.readFileSync("plugins.json");
  const o = JSON.parse(s);
  let config = parse(o);
  console.log(JSON.stringify(config));
}
