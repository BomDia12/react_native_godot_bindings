(() => {
  const ui = globalThis.nativeFabricUIManager;
  const handles = [{}, {}];
  let rootTag;
  let leaf;
  let sibling;

  const commitChildren = childrenToCommit => {
    const children = ui.createChildSet(rootTag);
    childrenToCommit.forEach(child => ui.appendChildToSet(children, child));
    ui.completeRoot(rootTag, children);
  };
  const commit = () => commitChildren([leaf, sibling]);

  globalThis.__godotMountingRegressionEvents = [];
  globalThis.__godotInvalidShadowLookup =
    ui.findShadowNodeByTag_DEPRECATED(1e100) === null;
  globalThis.__godotMountingRegression = (action, tag) => {
    switch (action) {
      case 'setup':
        rootTag = tag;
        ui.registerEventHandler((target, name) => {
          globalThis.__godotMountingRegressionEvents.push(name);
        });
        leaf = ui.createNode(90002, 'RCTView', rootTag, {
          width: 100, height: 100, opacity: 1,
        }, handles[0]);
        sibling = ui.createNode(90004, 'RCTView', rootTag, {
          width: 20, height: 20,
        }, handles[1]);
        commit();
        break;
      case 'declarative':
        leaf = ui.cloneNodeWithNewProps(leaf, {opacity: 0.7});
        commit();
        break;
      case 'direct':
        ui.setNativeProps(leaf, {opacity: 0.4});
        break;
      case 'sibling':
        sibling = ui.cloneNodeWithNewProps(sibling, {width: 30});
        commit();
        break;
      case 'clone':
        leaf = ui.cloneNode(leaf);
        commit();
        break;
      case 'coalesce':
        leaf = ui.cloneNodeWithNewProps(leaf, {opacity: 0.6});
        commit();
        leaf = ui.cloneNodeWithNewProps(leaf, {width: 110});
        commit();
        break;
      case 'roundtrip':
        leaf = ui.cloneNodeWithNewProps(leaf, {opacity: 0.8});
        commit();
        leaf = ui.cloneNodeWithNewProps(leaf, {opacity: 0.6});
        leaf = ui.cloneNodeWithNewChildren(leaf);
        commit();
        break;
      case 'reject':
        leaf = ui.cloneNodeWithNewProps(leaf, {opacity: 0.9});
        commit();
        break;
      case 'recover':
        leaf = ui.cloneNodeWithNewProps(leaf, {width: 120});
        commit();
        break;
      case 'clear':
        ui.setNativeProps(leaf, {opacity: null});
        break;
      case 'remove-leaf':
        commitChildren([sibling]);
        break;
      case 'restore-leaf':
        commit();
        break;
      case 'clear-events':
        globalThis.__godotMountingRegressionEvents.length = 0;
        break;
    }
  };
})();
