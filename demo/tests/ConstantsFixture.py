# Run this scene headlessly; a failed check exits Godot with code 1.
from godot import *
from godot.classes import Node


class ConstantsFixture(Extends(Node)):
    def _ready(self):
        try:
            self.check_imports()
        except Exception as exc:
            import traceback
            traceback.print_exc()
            print('CONSTANTS FAIL:', exc)
            self.owner.get_tree().quit(1)
            return
        print('CONSTANTS PASS')
        self.owner.get_tree().quit()

    def check_imports(self):
        import godot
        from godot.constants import KEY_SPACE, TYPE_BOOL, MOUSE_BUTTON_LEFT, PROPERTY_HINT_RANGE, OK, KEY_MASK_SHIFT

        constant_names = ['KEY_SPACE', 'TYPE_BOOL', 'MOUSE_BUTTON_LEFT', 'PROPERTY_HINT_RANGE', 'OK', 'KEY_MASK_SHIFT']
        assert 'KEY_SPACE' not in globals()

        assert KEY_SPACE == 32
        assert TYPE_BOOL == 1
        assert MOUSE_BUTTON_LEFT == 1
        assert PROPERTY_HINT_RANGE == 1
        assert OK == 0
        assert KEY_MASK_SHIFT == 1 << 25

        root_scope = {}
        exec('from godot import *', root_scope)
        assert 'constants' not in root_scope
        for name in constant_names:
            assert name not in root_scope
        assert 'Input' in root_scope
        assert 'Vector2' in root_scope
        assert 'Extends' in root_scope
        assert 'export' in root_scope

        old_import_failed = False
        try:
            exec('from godot import KEY_SPACE', {})
        except (ImportError, AttributeError):
            old_import_failed = True
        assert old_import_failed
        assert not hasattr(godot, 'KEY_SPACE')

        # Class constants and calls accepting global enum values still work.
        assert Vector2.ZERO.x == 0.0
        assert Node.PROCESS_MODE_ALWAYS == 3
        assert Input.is_key_pressed(KEY_SPACE) == False
        print('CONSTANTS CHECKED:', len(constant_names), 'selected constants;', len(root_scope), 'root entries')
