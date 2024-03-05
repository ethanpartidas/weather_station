import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'dart:io';

BluetoothDevice? device;
BluetoothService? service;
BluetoothCharacteristic? sensorData;
BluetoothCharacteristic? ssid;
BluetoothCharacteristic? password;

Future<void> bluetoothConnect() async {
  await device?.connect();
  service = (await device?.discoverServices())?[2];

  sensorData = service?.characteristics[0];
  ssid = service?.characteristics[1];
  password = service?.characteristics[2];

  await sensorData?.setNotifyValue(true);
  return;
}

void bluetoothDisconnect() {
  device?.disconnect();
}

bluetoothRead() async {
  return sensorData?.read();
}

void bluetoothOnNotification(void Function(List<int>?) cb) {
  sensorData?.onValueReceived.listen(cb);
}

Future<void> bluetoothSetSSID(String ssidValue) async {
  await ssid?.write(ssidValue.codeUnits);
  return;
}

Future<void> bluetoothSetPassword(String passwordValue) async {
  await password?.write(passwordValue.codeUnits);
  return;
}

void bluetoothInit() async {
  if (await FlutterBluePlus.isSupported == false) {
      print("Bluetooth not supported by this device");
      return;
  }

  FlutterBluePlus.adapterState.listen((BluetoothAdapterState state) {
      print(state);
      if (state == BluetoothAdapterState.on) {
          bluetoothScan();
      } else {
          // show an error to the user, etc
      }
  });

  if (Platform.isAndroid) {
      FlutterBluePlus.turnOn();
  }
}

void bluetoothScan() async {
  var subscription = FlutterBluePlus.onScanResults.listen(
    (results) {
        if (results.isNotEmpty) {
            ScanResult r = results.last;
            print('${r.device.remoteId}: "${r.advertisementData.advName}" found!');
            device = r.device;
        }
    },
    onError: (e) => print(e),
  );

  FlutterBluePlus.cancelWhenScanComplete(subscription);

  FlutterBluePlus.startScan(
    withNames:["Weather Station"],
    timeout: Duration(seconds:10),
    androidUsesFineLocation: true,
  );
}