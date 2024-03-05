import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Flutter Demo',
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: const Color(0xFF87CEEB)),
        useMaterial3: true,
      ),
      home: const MyHomePage(title: 'Weather Station'),
      debugShowCheckedModeBanner: false,
    );
  }
}

class MyHomePage extends StatefulWidget {
  const MyHomePage({super.key, required this.title});

  final String title;

  @override
  State<MyHomePage> createState() => _MyHomePageState();
}

class _MyHomePageState extends State<MyHomePage> {
  bool connected = false;

  void toggleConnected() {
    setState(() {
      connected = !connected;
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Stack(
        fit: StackFit.expand,
        children: <Widget>[
          Image.asset(
            "assets/background2.jpeg",
            fit: BoxFit.cover,
          ),
          Container(
            color: Colors.black.withOpacity(0.1), // Semi-transparent black color
          ),
          Center(
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: <Widget>[
                const SizedBox(height: 60),
                const MyText(text: 'Temperature: 76°F'),
                const MyText(text: 'Humidity: 52%'),
                const SizedBox(height: 40),
                const MyTextField(hint: "SSID"),
                const MyTextField(hint: "Password"),
                const SizedBox(height: 60),
                Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    MyButton(text: connected ? "Disconnect" : "Connect", onPressed: toggleConnected,),
                    const SizedBox(width: 10),
                    MyButton(text: "Upload", onPressed: () {}, enabled: connected,),
                  ],
                ),
              ],
            ),
          ),
          const Positioned(
            top: 65,
            right: 15,
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.end,
              children: <Widget>[
                MyText(
                  text: "March 5th",
                  borderColor: Colors.lightBlue,
                ),
                MyText(
                  text: "2024",
                  borderColor: Colors.lightBlue,
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class MyText extends StatelessWidget {
  const MyText({
    super.key,
    required this.text,
    this.borderColor = Colors.black,
  });

  final String text;
  final Color borderColor;

  @override
  Widget build(BuildContext context) {
    return Text(
      text,
      style: TextStyle(
        fontSize: 26,
        color: Colors.white,
        fontWeight: FontWeight.bold,
        shadows: [
          Shadow(
            color: borderColor, // Set the color of the border
            blurRadius: 5, // blur radius of the border
          ),
        ],
      ),
    );
  }
}

class MyTextField extends StatelessWidget {
  const MyTextField({
    super.key,
    required this.hint,
  });

  final String hint;

  @override
  Widget build(BuildContext context) {
    return Container(
      width: 220,
      height: 50,
      padding: const EdgeInsets.all(5),
      margin: const EdgeInsets.all(1),
      decoration: BoxDecoration(
        color: const Color(0x1f000000),
        borderRadius: BorderRadius.circular(5),
      ),
      child: TextField(
        decoration: InputDecoration(hintText: hint),
      ),
    );
  }
}

class MyButton extends StatelessWidget {
  const MyButton({
    super.key,
    required this.text,
    required this.onPressed,
    this.enabled = true,
  });

  final String text;
  final VoidCallback onPressed;
  final bool enabled;

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      width: 130,
      height: 40,
      child: FilledButton(
        onPressed: onPressed,
        style: FilledButton.styleFrom(
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(5),
          ),
          backgroundColor: enabled ? const Color(0x3f000000) : const Color(0x1f000000),
          padding: EdgeInsets.zero,
          foregroundColor: enabled ? const Color(0xdfffffff) : const Color(0x7fffffff),
        ),
        child: Text(
          text,
          style: const TextStyle(
            fontSize: 22,
          ),  
        ),
      ),
    );
  }
}
