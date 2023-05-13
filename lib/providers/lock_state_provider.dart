import 'package:hooks_riverpod/hooks_riverpod.dart';

final lockStateProvider =
    StateNotifierProvider((ref) => LockStateProvider(false));

class LockStateProvider extends StateNotifier<bool> {
  LockStateProvider(super.state);

  void setLockState(bool newState) {
    state = newState;
  }
}
