import 'dart:async';
import 'package:hooks_riverpod/hooks_riverpod.dart';
import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_server_client.dart';

final mqttProvider = Provider((_) => MqttProvider());

/// Riverpod provider that publishes and subscribes to mqtt topics in the background
class MqttProvider {
  final client = MqttServerClient('test.mosquitto.org', 'testingLock3');
  bool clientConnected = false;

  ///Connects to the mqtt client
  Future<void> connect() async {
    client.autoReconnect = true;
    // client.logging(on: true);
    await client.connect();
    clientConnected = true;
  }

  ///Subscribes to the mqtt topic and uses a function when it receives that data
  Future<void> subscribe(String topic, void Function(String) onData) async {
    Future.delayed(const Duration(seconds: 3),
        (() => {client.subscribe(topic, MqttQos.atMostOnce)}));

    client.updates!.listen((List<MqttReceivedMessage<MqttMessage>> messages) {
      for (MqttReceivedMessage message in messages) {
        var payload = message.payload.payload.message;
        var data = String.fromCharCodes(payload);
        onData(data);
      }
    });
  }

  ///Publishes a data string to a topic string on the mqtt client
  Future<void> publish(String topic, String data) async {
    final builder = MqttClientPayloadBuilder();
    builder.addString(data);
    client.publishMessage(topic, MqttQos.atMostOnce, builder.payload!);
  }

  ///Disconnects from the mqtt client
  void disconnect() {
    client.disconnect();
  }
}
