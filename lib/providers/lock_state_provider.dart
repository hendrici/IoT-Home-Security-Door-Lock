import 'package:hooks_riverpod/hooks_riverpod.dart';

final lockStateProvider =
    StateNotifierProvider((ref) => LockStateProvider(false));

///A provider that holds what known state the lock is in
class LockStateProvider extends StateNotifier<bool> {
  LockStateProvider(super.state);

  ///Updates the lock state to newState
  void setLockState(bool newState) {
    state = newState;
  }
}
