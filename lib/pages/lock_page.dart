import 'package:flutter/material.dart';
import 'package:flutter_hooks/flutter_hooks.dart';
import 'package:hooks_riverpod/hooks_riverpod.dart';
import 'package:lock_app/providers/lock_state_provider.dart';
import 'package:lock_app/providers/mqtt_provider.dart';

class LockPage extends HookConsumerWidget {
  const LockPage({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    ref.read(mqttProvider).connect();
    ref.read(mqttProvider).subscribe(
        "brendan/lockStatus/",
        ((data) => ref
            .read(lockStateProvider.notifier)
            .setLockState(data.contains("true"))));
    return AppBar(
      title: const Text("Lock page"),
      flexibleSpace: Padding(
        padding: const EdgeInsets.all(8),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          crossAxisAlignment: CrossAxisAlignment.center,
          children: [
            Consumer(
              builder: (context, ref, child) {
                final lockState = ref.watch(lockStateProvider);
                return Icon(
                  lockState != null ? Icons.lock : Icons.lock_open,
                  size: 100,
                );
              },
            ),
            const Text("Enter a pin"),
          ],
        ),
      ),
    );
  }
}
