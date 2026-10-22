import React, {useEffect} from 'react';
import {AppRegistry, ScrollView, Text, View} from 'react-native';
import 'react-native-godot/setup';

const rendered = global.__fidelity = {};

function Screen({name, children}) {
  useEffect(() => {
    rendered[name] = true;
  }, [name]);
  return <View style={{width: 480, height: 360, backgroundColor: '#f4f1ea', padding: 12, flexDirection: 'row', flexWrap: 'wrap', gap: 12}}>{children}</View>;
}

const tile = {width: 72, height: 72, backgroundColor: '#ffffff'};

function StyleGallery() {
  return <Screen name="StyleGallery">
    <View style={{...tile, borderTopWidth: 4, borderRightWidth: 6, borderBottomWidth: 8, borderLeftWidth: 2, borderTopColor: '#d1495b', borderRightColor: '#edae49', borderBottomColor: '#00798c', borderLeftColor: '#30638e'}} />
    <View style={{...tile, borderWidth: 3, borderColor: '#30638e', borderStyle: 'dashed'}} />
    <View style={{...tile, borderWidth: 3, borderColor: '#00798c', borderStyle: 'dotted'}} />
    <View style={{...tile, borderWidth: 2, borderColor: '#003d5b', borderTopLeftRadius: 24, borderTopRightRadius: 4, borderBottomRightRadius: 36, borderBottomLeftRadius: 12}} />
    <View style={{...tile, borderWidth: 2, borderColor: '#003d5b', borderRadius: '30%'}} />
    <View style={{...tile, borderRadius: 12, boxShadow: '4px 6px 8px 2px rgba(0, 0, 0, 0.35)'}} />
    <View style={{...tile, borderRadius: 12, boxShadow: 'inset 0px 4px 10px 4px rgba(209, 73, 91, 0.6)'}} />
    <View style={{...tile, borderRadius: 12, boxShadow: '0px 0px 0px 4px #edae49, 6px 6px 0px 0px #30638e'}} />
    <View style={{...tile, backgroundImage: 'linear-gradient(135deg, #d1495b, #edae49 50%, #00798c)'}} />
    <View style={{...tile, borderRadius: 36, backgroundImage: 'radial-gradient(circle at 30% 30%, #ffffff, #30638e)'}} />
    <View style={{...tile, outlineWidth: 3, outlineColor: '#d1495b', outlineOffset: 4, outlineStyle: 'dashed'}} />
    <View style={{...tile, transform: [{rotate: '15deg'}, {scale: 0.8}], transformOrigin: 'top left', backgroundColor: '#00798c'}} />
    <View style={{...tile, borderRadius: 18, overflow: 'hidden'}}>
      <ScrollView contentOffset={{x: 0, y: 24}}>
        <View style={{height: 48, backgroundColor: '#d1495b'}} />
        <View style={{height: 48, backgroundColor: '#edae49'}} />
      </ScrollView>
    </View>
    <View style={{...tile, backgroundColor: 'transparent'}}>
      <View style={{position: 'absolute', left: 0, top: 0, width: 48, height: 48, backgroundColor: '#30638e', zIndex: 2}} />
      <View style={{position: 'absolute', left: 16, top: 16, width: 48, height: 48, backgroundColor: '#edae49', zIndex: 1}} />
      <View style={{position: 'absolute', left: 32, top: 32, width: 40, height: 40, backgroundColor: '#d1495b', zIndex: -1}} />
    </View>
    <View style={{...tile, justifyContent: 'center', alignItems: 'center'}}><Text style={{color: '#003d5b', fontSize: 14}}>Text</Text></View>
  </Screen>;
}

AppRegistry.registerComponent('StyleGallery', () => StyleGallery);
