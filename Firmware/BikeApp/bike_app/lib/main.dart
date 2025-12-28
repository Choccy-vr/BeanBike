import 'package:flutter/material.dart';
// ignore: unused_import
import 'package:window_manager/window_manager.dart';

//import 'UART.dart';
import '/pages/main_page.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  //uncomment at build
  await windowManager.ensureInitialized();

  WindowOptions windowOptions = WindowOptions(fullScreen: true);
  windowManager.waitUntilReadyToShow(windowOptions, () async {
    await windowManager.show();
    await windowManager.focus();
  });

  runApp(const MainApp());
}

class MainApp extends StatefulWidget {
  const MainApp({super.key});

  @override
  State<MainApp> createState() => _MainAppState();
}

class _MainAppState extends State<MainApp> {
  ThemeMode _themeMode = ThemeMode.light;
  final Color _seedColor = const Color.fromARGB(255, 14, 142, 60);
  //late final UARTService _uartService;

  @override
  void initState() {
    super.initState();
    //_uartService = UARTService(devicePath: '/dev/serial0');
    //_uartService.open();
  }

  @override
  void dispose() {
    //_uartService.dispose();
    super.dispose();
  }

  void _toggleThemeMode() {
    setState(() {
      _themeMode = _themeMode == ThemeMode.light
          ? ThemeMode.dark
          : ThemeMode.light;
    });
  }

  @override
  Widget build(BuildContext context) {
    final ColorScheme light = ColorScheme.fromSeed(
      seedColor: _seedColor,
      brightness: Brightness.light,
    );
    final ColorScheme dark = ColorScheme.fromSeed(
      seedColor: _seedColor,
      brightness: Brightness.dark,
    );

    return ThemeController(
      themeMode: _themeMode,
      toggleTheme: _toggleThemeMode,
      child: MaterialApp(
        title: 'Boot App',
        theme: ThemeData(useMaterial3: true, colorScheme: light),
        darkTheme: ThemeData(useMaterial3: true, colorScheme: dark),
        themeMode: _themeMode,
        home: MainPage(/*uartService: _uartService*/),
        debugShowCheckedModeBanner: false,
      ),
    );
  }
}

class ThemeController extends InheritedWidget {
  final ThemeMode themeMode;
  final VoidCallback toggleTheme;

  const ThemeController({
    super.key,
    required this.themeMode,
    required this.toggleTheme,
    required super.child,
  });

  static ThemeController? maybeOf(BuildContext context) {
    return context.dependOnInheritedWidgetOfExactType<ThemeController>();
  }

  static ThemeController of(BuildContext context) {
    final ThemeController? result = maybeOf(context);
    assert(result != null, 'No ThemeController found in context');
    return result!;
  }

  @override
  bool updateShouldNotify(ThemeController oldWidget) {
    return themeMode != oldWidget.themeMode;
  }
}
