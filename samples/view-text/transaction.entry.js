import React from 'react';
import {AppRegistry, Text, View} from 'react-native';

let updateTransactionApp = null;

const initialState = {
  items: ['alpha', 'beta', 'gamma'],
  alphaText: 'alpha',
  wide: false,
  recovery: 0,
};

function TransactionApp() {
  const [state, setState] = React.useState(initialState);
  const alphaRef = React.useRef(null);
  updateTransactionApp = action => {
    setState(current => {
      switch (action) {
        case 'update-leaf':
          return {...current, alphaText: 'alpha*'};
        case 'reorder':
          return {...current, items: ['gamma', 'alpha', 'beta']};
        case 'remove':
          return {...current, items: current.items.filter(item => item !== 'beta')};
        case 'insert':
          return {...current, items: [...current.items, 'delta']};
        case 'style':
          return {...current, items: ['alpha', 'gamma', 'delta'], wide: true};
        case 'recover':
          return {...current, recovery: current.recovery + 1};
        default:
          return current;
      }
    });
  };
  global.__godotTransactionPropsAfterFailure = () => {
    alphaRef.current?.setNativeProps({opacity: 0.4});
  };

  return (
    <View style={{flex: 1, flexDirection: 'row', gap: 4, opacity: state.recovery ? 0.99 : 1}}>
      {state.items.map(item => (
        <View
          key={item}
          ref={item === 'alpha' ? alphaRef : null}
          focusable={item === 'alpha'}
          pointerEvents={state.wide && item === 'alpha' ? 'box-only' : 'auto'}
          style={{width: state.wide && item === 'alpha' ? 72 : 56, height: 40, borderWidth: 1}}>
          <Text>{item === 'alpha' ? state.alphaText : item}</Text>
        </View>
      ))}
    </View>
  );
}

global.__godotTransactionAction = action => {
  updateTransactionApp?.(action);
};

AppRegistry.registerComponent('GodotTransactionApp', () => TransactionApp);
