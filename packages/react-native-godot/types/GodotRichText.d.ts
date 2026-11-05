import type {Component, ComponentType, ElementRef, RefAttributes} from 'react';
import type {NativeSyntheticEvent, ViewProps} from 'react-native';
import '../godot';

export type GodotRichTextProps = ViewProps & {
  // RichTextLabel BBCode.
  bbcode?: string;
  // res:// paths to RichTextEffect resources.
  customEffects?: readonly string[];
  visibleRatio?: number;
  visibleCharactersBehavior?: 'charsBeforeShaping' | 'charsAfterShaping' | 'glyphsLayoutDirection' | 'glyphsLeftToRight' | 'glyphsRightToLeft';
  selectionEnabled?: boolean;
  scrollActive?: boolean;
  onMetaClick?: (event: NativeSyntheticEvent<{meta: string}>) => void;
  onMetaHoverStarted?: (event: NativeSyntheticEvent<{meta: string}>) => void;
  onMetaHoverEnded?: (event: NativeSyntheticEvent<{meta: string}>) => void;
  onRevealFinished?: (event: NativeSyntheticEvent<null>) => void;
};

declare const GodotRichText: ComponentType<GodotRichTextProps & RefAttributes<any>>;
export default GodotRichText;
export const GodotRichTextCommands: {
  // Animates the visible ratio natively, Godot's typewriter effect.
  reveal(view: ElementRef<typeof Component>, toRatio: number, seconds: number): void;
};
