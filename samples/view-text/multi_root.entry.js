import React, {useRef, useState} from 'react';
import {
  AppRegistry,
  findNodeHandle,
  StyleSheet,
  Text,
  unstable_batchedUpdates,
  View,
} from 'react-native';

const roots = {};
const savedRefs = {};
global.__godotMultiRootSnapshot = {};
global.__godotMultiTimerTicks = 0;
setInterval(() => {
  global.__godotMultiTimerTicks += 1;
}, 0);

function publish(side, count, renders, events, rootTag) {
  global.__godotMultiRootSnapshot[side] = {
    count,
    renders,
    events,
    rootTag,
  };
}

class CompositeTarget extends React.Component {
  render() {
    const {hostRef, ...props} = this.props;
    return <View ref={hostRef} {...props} />;
  }
}

function Fixture({side, color, rootTag}) {
  const [count, setCount] = useState(0);
  const [declarativeStyle, setDeclarativeStyle] = useState(null);
  const containerRef = useRef(null);
  const compositeRef = useRef(null);
  const siblingRef = useRef(null);
  const targetRef = useRef(null);
  const eventsRef = useRef([]);
  const rendersRef = useRef(0);
  rendersRef.current += 1;

  roots[side] = {
    containerRef,
    compositeRef,
    siblingRef,
    targetRef,
    setCount,
    setDeclarativeStyle,
    events: eventsRef.current,
  };
  publish(side, count, rendersRef.current, eventsRef.current, rootTag);

  return (
    <View ref={containerRef} nativeID={`${side}-container`} style={styles.root}>
      <CompositeTarget
        ref={compositeRef}
        hostRef={targetRef}
        nativeID={`${side}-target`}
        focusable
        onClick={() => setCount(value => value + 1)}
        onFocus={() => eventsRef.current.push('focus')}
        onBlur={() => eventsRef.current.push('blur')}
        onGotPointerCapture={() => eventsRef.current.push('got-capture')}
        onLostPointerCapture={() => eventsRef.current.push('lost-capture')}
        onLayout={event =>
          eventsRef.current.push(`layout:${event.nativeEvent.layout.width}`)
        }
        onPointerDown={event => {
          targetRef.current?.setPointerCapture(event.nativeEvent.pointerId);
          eventsRef.current.push(
            `capture-pending:${targetRef.current?.hasPointerCapture(event.nativeEvent.pointerId)}`,
          );
        }}
        style={[
          styles.target,
          {backgroundColor: color},
          declarativeStyle,
        ]}>
        <Text>{`${side}:${count}`}</Text>
      </CompositeTarget>
      <View ref={siblingRef} nativeID={`${side}-sibling`} style={styles.sibling} />
    </View>
  );
}

const createFixture = (side, color) => props => (
  <Fixture side={side} color={color} {...props} />
);

function BrokenFixture() {
  throw new Error('intentional surface failure');
}

AppRegistry.registerComponent('GodotLeftApp', () =>
  createFixture('left', '#884422'),
);
AppRegistry.registerComponent('GodotRightApp', () =>
  createFixture('right', '#225588'),
);
AppRegistry.registerComponent('GodotBrokenApp', () => BrokenFixture);

global.__godotMultiRootProbe = side => {
  const root = roots[side];
  const container = root?.containerRef.current;
  const sibling = root?.siblingRef.current;
  const target = root?.targetRef.current;
  const other = roots[side === 'left' ? 'right' : 'left']?.targetRef.current;
  if (container == null || sibling == null || target == null) {
    return null;
  }

  let measure = null;
  let windowMeasure = null;
  let layoutMeasure = null;
  target.measure((x, y, width, height, pageX, pageY) => {
    measure = [x, y, width, height, pageX, pageY];
  });
  target.measureInWindow((x, y, width, height) => {
    windowMeasure = [x, y, width, height];
  });
  target.measureLayout(
    container,
    (x, y, width, height) => {
      layoutMeasure = [x, y, width, height];
    },
    () => {
      layoutMeasure = 'failed';
    },
  );
  const bounds = target.getBoundingClientRect();

  return {
    tagName: target.tagName,
    textContent: target.textContent,
    childCount: target.childNodes.length,
    parentMatches: target.parentNode === container,
    byIdMatches:
      target.ownerDocument.getElementById(`${side}-target`) === target,
    connected: target.isConnected,
    documentConnected: target.ownerDocument.isConnected,
    documentElementParentMatches:
      target.ownerDocument.documentElement.parentNode === target.ownerDocument,
    position: container.compareDocumentPosition(target),
    samePosition: target.compareDocumentPosition(target),
    documentPosition: target.ownerDocument.compareDocumentPosition(
      target.ownerDocument,
    ),
    crossPosition: target.compareDocumentPosition(other),
    followingPosition: target.compareDocumentPosition(sibling),
    precedingPosition: sibling.compareDocumentPosition(target),
    bounds: [bounds.x, bounds.y, bounds.width, bounds.height],
    clientWidth: target.clientWidth,
    clientHeight: target.clientHeight,
    offsetLeft: target.offsetLeft,
    offsetTop: target.offsetTop,
    measure,
    windowMeasure,
    layoutMeasure,
    handle: findNodeHandle(target),
    compositeHandle: findNodeHandle(root.compositeRef.current),
    numericHandle: findNodeHandle(123),
    nullHandle: findNodeHandle(null),
  };
};

global.__godotMultiRootSaveRef = side => {
  savedRefs[side] = roots[side]?.targetRef.current;
  return findNodeHandle(savedRefs[side]);
};

global.__godotMultiRootStaleHandle = side => findNodeHandle(savedRefs[side]);

global.__godotMultiRootStaleProbe = side => {
  const target = savedRefs[side];
  let measure = null;
  target.measure((...values) => {
    measure = values;
  });
  const bounds = target.getBoundingClientRect();
  return {
    connected: target.isConnected,
    tagName: target.tagName,
    textContent: target.textContent,
    childCount: target.childNodes.length,
    parentMissing: target.parentNode == null,
    zeroBounds:
      bounds.x === 0 &&
      bounds.y === 0 &&
      bounds.width === 0 &&
      bounds.height === 0,
    measureSkipped: measure == null,
  };
};

global.__godotMultiRootSetProps = side => {
  roots[side]?.targetRef.current?.setNativeProps({
    style: {opacity: 0.4, width: 100},
  });
};

global.__godotMultiRootSetDeclarativeOpacity = side => {
  roots[side]?.setDeclarativeStyle({opacity: 0.7, width: 95});
};

global.__godotMultiRootFocus = side => {
  roots[side]?.targetRef.current?.focus();
};

global.__godotMultiRootBlur = side => {
  roots[side]?.targetRef.current?.blur();
};

global.__godotMultiRootBatchedIncrement = side => {
  const root = roots[side];
  unstable_batchedUpdates(() => {
    root?.setCount(value => value + 1);
    root?.setCount(value => value + 1);
  });
};

const styles = StyleSheet.create({
  root: {
    flex: 1,
    padding: 8,
  },
  target: {
    width: 90,
    height: 44,
    borderWidth: 2,
    padding: 4,
  },
  sibling: {
    width: 1,
    height: 1,
  },
});
