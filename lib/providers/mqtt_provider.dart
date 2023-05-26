import 'dart:async';
import 'package:hooks_riverpod/hooks_riverpod.dart';
import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_server_client.dart';

final mqttProvider = Provider((_) => MqttProvider());

class MqttProvider {
  final client = MqttServerClient('test.mosquitto.org', 'testingLock3');
  bool clientConnected = false;

  Future<void> connect() async {
    client.autoReconnect = true;
    // client.logging(on: true);
    await client.connect();
    clientConnected = true;
  }

  Future<void> subscribe(String topic, void Function(String) onData) async {
    Future.delayed(const Duration(seconds: 3),
        (() => {client.subscribe(topic, MqttQos.atMostOnce)}));

    client.updates!.listen((List<MqttReceivedMessage<MqttMessage>> messages) {
      for (MqttReceivedMessage message in messages) {
        var payload = message.payload.payload.message;
        var data = String.fromCharCodes(payload);
        print(message.payload.toString());
        print(message.payload);
        onData(data);
      }
    });
  }

  Future<void> publish(String topic, String data) async {
    final builder = MqttClientPayloadBuilder();
    builder.addString(data);
    client.publishMessage(topic, MqttQos.atMostOnce, builder.payload!);
  }

  void disconnect() {
    client.disconnect();
  }
}
