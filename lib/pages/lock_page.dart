import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_hooks/flutter_hooks.dart';
import 'package:hooks_riverpod/hooks_riverpod.dart';
import 'package:lock_app/providers/lock_state_provider.dart';
import 'package:lock_app/providers/mqtt_provider.dart';

const lockStatusTopic = "brendan/lockStatus/";
const pinOutputTopic = "brendan/pinEntry/";

class LockPage extends HookConsumerWidget {
  const LockPage({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    var pinInput = "none";
    ref.read(mqttProvider).connect();
    ref.read(mqttProvider).subscribe(
        lockStatusTopic,
        ((data) => ref
            .read(lockStateProvider.notifier)
            .setLockState(data.contains("unlocked"))));
    return AppBar(
      title: const Text("Lock page"),
      flexibleSpace: Padding(
        padding: const EdgeInsets.all(8),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.start,
          crossAxisAlignment: CrossAxisAlignment.center,
          children: [
            const SizedBox(
              height: 200,
            ),
            Consumer(
              builder: (context, ref, child) {
                var lockState = ref.watch(lockStateProvider);
                return Icon(
                  lockState as bool ? Icons.lock_open : Icons.lock,
                  size: 100,
                );
              },
            ),
            Padding(
              padding: const EdgeInsets.fromLTRB(32, 5, 32, 5),
              child: TextFormField(
                inputFormatters: [FilteringTextInputFormatter.digitsOnly],
                decoration: const InputDecoration(
                  hintText: 'Enter a pin',
                  labelText: 'Pin',
                ),
                onChanged: (String? value) {
                  pinInput = value ?? "none";
                },
                validator: (String? value) {
                  return (value != null) ? 'Do not use the @ char.' : null;
                },
              ),
            ),
            ElevatedButton(
                onPressed: () =>
                    ref.read(mqttProvider).publish(pinOutputTopic, pinInput),
                child: const Text("Send")),
          ],
        ),
      ),
    );
  }
}
