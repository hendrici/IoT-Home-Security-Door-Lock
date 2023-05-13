import 'dart:async';
import 'package:hooks_riverpod/hooks_riverpod.dart';
import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_server_client.dart';

final mqttProvider = Provider((_) => MqttProvider());

class MqttProvider {
  late MqttServerClient _client;
  bool clientConnected = false;

  Future<void> connect() async {
    final client = MqttServerClient('test.mosquitto.org', 'testingLock');
    client.autoReconnect = true;
    client.logging(on: true);
    await client.connect();
    _client = client;
    clientConnected = true;
  }

  Future<void> subscribe(String topic, void Function(String) onData) async {
    while (clientConnected) {}
    _client.subscribe(topic, MqttQos.atMostOnce);
    _client.updates!.listen((List<MqttReceivedMessage<MqttMessage>> messages) {
      for (var message in messages) {
        final data = message.payload.toString();
        onData(data);
      }
    });
  }

  Future<void> publish(String topic, String data) async {
    final builder = MqttClientPayloadBuilder();
    builder.addString(data);
    _client.publishMessage(topic, MqttQos.atMostOnce, builder.payload!);
  }

  void disconnect() {
    _client.disconnect();
  }
}
