import 'dart:async';

import 'package:flutter/material.dart';

import '../UART.dart';

class MainPage extends StatefulWidget {
  const MainPage({super.key, required this.uartService});

  final UARTService uartService;

  @override
  State<MainPage> createState() => _MainPageState();
}

class _MainPageState extends State<MainPage> with WidgetsBindingObserver {
  String _currentTime = '';
  Timer? _timer;
  late final UARTService _uart;
  StreamSubscription<UARTFrame>? _frameSubscription;
  bool _isPasHovered = false;
  bool _isCruiseHovered = false;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    _uart = widget.uartService;
    _updateTime();
    _timer = Timer.periodic(const Duration(seconds: 1), (_) => _updateTime());

    if (_uart.pasLevel == null) {
      _uart.pasLevel = 0;
      _uart.pasModeEnabled = 0;
      unawaited(_uart.sendMsg('SET', 'PAS_LEVEL', '0'));
      unawaited(_uart.sendMsg('SET', 'PAS_MODE_ENABLED', '0'));
    }

    if (_uart.cruiseControlTarget == null) {
      _uart.cruiseControlTarget = 0;
      unawaited(_uart.sendMsg('SET', 'CRUISE_CONTROL_TARGET', '0'));
    }

    _uart.speedMph ??= 0;

    _frameSubscription = _uart.frames.listen((frame) {
      _uart
          .parseMsg(frame)
          .then((_) {
            if (!mounted) {
              return;
            }
            setState(() {});
          })
          .catchError((_) {
            // Ignore parse errors for now; they are surfaced via the stream.
          });
    });
  }

  @override
  void dispose() {
    _frameSubscription?.cancel();
    _timer?.cancel();
    WidgetsBinding.instance.removeObserver(this);
    super.dispose();
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (state == AppLifecycleState.resumed ||
        state == AppLifecycleState.inactive) {
      _updateTime();
    }
  }

  void _updateTime() {
    final now = DateTime.now();
    final hour = now.hour == 0
        ? 12
        : (now.hour > 12 ? now.hour - 12 : now.hour);
    final period = now.hour >= 12 ? 'PM' : 'AM';
    if (!mounted) {
      return;
    }
    setState(() {
      _currentTime =
          '${hour.toString().padLeft(2, '0')}:${now.minute.toString().padLeft(2, '0')}:${now.second.toString().padLeft(2, '0')} $period';
    });
  }

  void _showNumberPad() {
    String tempSpeed = ((_uart.cruiseControlTarget ?? 0).round()).toString();
    showDialog(
      context: context,
      builder: (context) {
        return StatefulBuilder(
          builder: (context, setDialogState) {
            return AlertDialog(
              title: const Text('Set Cruise Speed'),
              content: SizedBox(
                width: 300,
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Text(
                      '$tempSpeed MPH',
                      style: Theme.of(context).textTheme.headlineMedium
                          ?.copyWith(
                            color: Theme.of(context).colorScheme.primary,
                            fontWeight: FontWeight.bold,
                          ),
                    ),
                    const SizedBox(height: 16),
                    Column(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        SizedBox(
                          width: 240,
                          height: 320,
                          child: GridView.count(
                            shrinkWrap: true,
                            crossAxisCount: 3,
                            mainAxisSpacing: 8,
                            crossAxisSpacing: 8,
                            physics: const NeverScrollableScrollPhysics(),
                            children: [
                              for (int i = 1; i <= 9; i++)
                                ElevatedButton(
                                  onPressed: () {
                                    if (tempSpeed.length < 2) {
                                      final newSpeed = tempSpeed + i.toString();
                                      final speedValue = int.tryParse(newSpeed);
                                      if (speedValue != null &&
                                          speedValue <= 25) {
                                        setDialogState(() {
                                          tempSpeed = newSpeed;
                                        });
                                      }
                                    }
                                  },
                                  child: Text(
                                    i.toString(),
                                    style: const TextStyle(fontSize: 24),
                                  ),
                                ),
                              const SizedBox.shrink(),
                              ElevatedButton(
                                onPressed: () {
                                  if (tempSpeed.length < 2) {
                                    final newSpeed = tempSpeed + '0';
                                    final speedValue = int.tryParse(newSpeed);
                                    if (speedValue != null &&
                                        speedValue <= 25) {
                                      setDialogState(() {
                                        tempSpeed = newSpeed;
                                      });
                                    }
                                  }
                                },
                                child: const Text(
                                  '0',
                                  style: TextStyle(fontSize: 24),
                                ),
                              ),
                              ElevatedButton(
                                onPressed: () {
                                  if (tempSpeed.isNotEmpty) {
                                    setDialogState(() {
                                      tempSpeed = tempSpeed.substring(
                                        0,
                                        tempSpeed.length - 1,
                                      );
                                    });
                                  }
                                },
                                child: const Icon(Icons.backspace),
                              ),
                            ],
                          ),
                        ),
                        const SizedBox(height: 8),
                        SizedBox(
                          width: 240,
                          height: 56,
                          child: ElevatedButton(
                            onPressed: () {
                              final speed = int.tryParse(tempSpeed);
                              if (speed != null && speed >= 0 && speed <= 25) {
                                setState(() {
                                  _uart.cruiseControlTarget = speed.toDouble();
                                });
                                unawaited(
                                  _uart.sendMsg(
                                    'SET',
                                    'CRUISE_CONTROL_TARGET',
                                    speed.toString(),
                                  ),
                                );
                              }
                              Navigator.pop(context);
                            },
                            style: ElevatedButton.styleFrom(
                              backgroundColor: Theme.of(
                                context,
                              ).colorScheme.primary,
                            ),
                            child: const Icon(Icons.check, size: 32),
                          ),
                        ),
                      ],
                    ),
                  ],
                ),
              ),
            );
          },
        );
      },
    );
  }

  @override
  Widget build(BuildContext context) {
    final colorScheme = Theme.of(context).colorScheme;
    final double speedValue = (_uart.speedMph ?? 0.0).clamp(0.0, 99.9);
    final String speedDisplay = speedValue.toStringAsFixed(1).padLeft(4, '0');
    final int pasLevel = _uart.pasLevel ?? 0;
    final int cruiseSpeed = (_uart.cruiseControlTarget ?? 0).round();
    final bool isCruiseControl = cruiseSpeed > 0;

    return Scaffold(
      backgroundColor: colorScheme.surface,
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          children: [
            Container(
              width: double.infinity,
              padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 20),
              decoration: BoxDecoration(
                color: colorScheme.surfaceContainerHighest,
                borderRadius: BorderRadius.circular(16),
                boxShadow: [
                  BoxShadow(
                    color: Colors.black.withAlpha(40),
                    blurRadius: 8,
                    offset: const Offset(0, 4),
                    spreadRadius: 0,
                  ),
                ],
              ),
              child: Row(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  Row(
                    children: [
                      Icon(
                        Icons.access_time,
                        size: 26,
                        color: colorScheme.onSurface,
                      ),
                      const SizedBox(width: 8),
                      Text(
                        _currentTime,
                        style: Theme.of(context).textTheme.bodyLarge?.copyWith(
                          color: colorScheme.onSurface,
                          fontWeight: FontWeight.w500,
                        ),
                      ),
                    ],
                  ),
                  Row(
                    children: [
                      Icon(
                        Icons.speed_rounded,
                        size: 32,
                        color: isCruiseControl
                            ? colorScheme.primary
                            : colorScheme.onSurface,
                      ),
                      const SizedBox(width: 12),
                      Icon(
                        Icons.bluetooth,
                        size: 32,
                        color: colorScheme.onSurface,
                      ),
                      const SizedBox(width: 12),
                      Icon(
                        Icons.wifi_rounded,
                        size: 32,
                        color: colorScheme.onSurface,
                      ),
                    ],
                  ),
                ],
              ),
            ),
            const SizedBox(height: 16),
            Expanded(
              child: Container(
                decoration: BoxDecoration(
                  color: colorScheme.surfaceContainer,
                  borderRadius: BorderRadius.circular(16),
                ),
                child: Stack(
                  children: [
                    Center(
                      child: Column(
                        mainAxisSize: MainAxisSize.min,
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(
                            speedDisplay,
                            style: Theme.of(context).textTheme.displayLarge
                                ?.copyWith(
                                  color: colorScheme.primary,
                                  fontWeight: FontWeight.bold,
                                  fontSize: 126,
                                ),
                          ),
                          Text(
                            'MPH',
                            style: Theme.of(context).textTheme.headlineSmall
                                ?.copyWith(
                                  color: colorScheme.onSurface,
                                  fontWeight: FontWeight.bold,
                                ),
                          ),
                        ],
                      ),
                    ),
                    Positioned(
                      bottom: 16,
                      right: 16,
                      child: GestureDetector(
                        onTap: () {
                          setState(() => _isPasHovered = !_isPasHovered);
                        },
                        child: MouseRegion(
                          onEnter: (_) => setState(() => _isPasHovered = true),
                          onExit: (_) => setState(() => _isPasHovered = false),
                          child: AnimatedContainer(
                            duration: const Duration(milliseconds: 300),
                            curve: Curves.easeInOut,
                            padding: EdgeInsets.symmetric(
                              horizontal: _isPasHovered ? 16 : 24,
                              vertical: _isPasHovered ? 12 : 16,
                            ),
                            decoration: BoxDecoration(
                              color: colorScheme.surfaceContainerHighest,
                              borderRadius: BorderRadius.circular(16),
                              boxShadow: [
                                BoxShadow(
                                  color: Colors.black.withAlpha(40),
                                  blurRadius: _isPasHovered ? 12 : 8,
                                  offset: const Offset(0, 4),
                                ),
                              ],
                            ),
                            child: Row(
                              mainAxisSize: MainAxisSize.min,
                              children: [
                                AnimatedSize(
                                  duration: const Duration(milliseconds: 300),
                                  curve: Curves.easeInOut,
                                  alignment: Alignment.centerLeft,
                                  child: _isPasHovered
                                      ? Row(
                                          mainAxisSize: MainAxisSize.min,
                                          children: [
                                            ElevatedButton(
                                              onPressed: pasLevel > 0
                                                  ? () {
                                                      final newLevel =
                                                          pasLevel - 1;
                                                      final modeEnabled =
                                                          newLevel > 0 ? 1 : 0;
                                                      setState(() {
                                                        _uart.pasLevel =
                                                            newLevel;
                                                        _uart.pasModeEnabled =
                                                            modeEnabled;
                                                      });
                                                      unawaited(
                                                        _uart.sendMsg(
                                                          'SET',
                                                          'PAS_LEVEL',
                                                          newLevel.toString(),
                                                        ),
                                                      );
                                                      unawaited(
                                                        _uart.sendMsg(
                                                          'SET',
                                                          'PAS_MODE_ENABLED',
                                                          modeEnabled
                                                              .toString(),
                                                        ),
                                                      );
                                                    }
                                                  : null,
                                              style: ElevatedButton.styleFrom(
                                                fixedSize: const Size(56, 56),
                                                padding: EdgeInsets.zero,
                                                shape: RoundedRectangleBorder(
                                                  borderRadius:
                                                      BorderRadius.circular(8),
                                                ),
                                              ),
                                              child: const Icon(
                                                Icons.remove,
                                                size: 32,
                                              ),
                                            ),
                                            const SizedBox(width: 16),
                                          ],
                                        )
                                      : const SizedBox(width: 0, height: 56),
                                ),
                                Column(
                                  mainAxisSize: MainAxisSize.min,
                                  children: [
                                    Text(
                                      'PAS',
                                      style: Theme.of(context)
                                          .textTheme
                                          .titleMedium
                                          ?.copyWith(
                                            color: colorScheme.onSurface,
                                            fontWeight: FontWeight.w600,
                                            fontSize: 18,
                                          ),
                                    ),
                                    const SizedBox(height: 4),
                                    Text(
                                      '$pasLevel',
                                      style: Theme.of(context)
                                          .textTheme
                                          .displayMedium
                                          ?.copyWith(
                                            color: colorScheme.primary,
                                            fontWeight: FontWeight.bold,
                                            fontSize: 48,
                                            height: 1.0,
                                          ),
                                    ),
                                  ],
                                ),
                                AnimatedSize(
                                  duration: const Duration(milliseconds: 300),
                                  curve: Curves.easeInOut,
                                  alignment: Alignment.centerRight,
                                  child: _isPasHovered
                                      ? Row(
                                          mainAxisSize: MainAxisSize.min,
                                          children: [
                                            const SizedBox(width: 16),
                                            ElevatedButton(
                                              onPressed: pasLevel < 5
                                                  ? () {
                                                      final newLevel =
                                                          pasLevel + 1;
                                                      setState(() {
                                                        _uart.pasLevel =
                                                            newLevel;
                                                        _uart.pasModeEnabled =
                                                            1;
                                                      });
                                                      unawaited(
                                                        _uart.sendMsg(
                                                          'SET',
                                                          'PAS_LEVEL',
                                                          newLevel.toString(),
                                                        ),
                                                      );
                                                      unawaited(
                                                        _uart.sendMsg(
                                                          'SET',
                                                          'PAS_MODE_ENABLED',
                                                          '1',
                                                        ),
                                                      );
                                                    }
                                                  : null,
                                              style: ElevatedButton.styleFrom(
                                                fixedSize: const Size(56, 56),
                                                padding: EdgeInsets.zero,
                                                shape: RoundedRectangleBorder(
                                                  borderRadius:
                                                      BorderRadius.circular(8),
                                                ),
                                              ),
                                              child: const Icon(
                                                Icons.add,
                                                size: 32,
                                              ),
                                            ),
                                          ],
                                        )
                                      : const SizedBox(width: 0, height: 56),
                                ),
                              ],
                            ),
                          ),
                        ),
                      ),
                    ),
                    Positioned(
                      bottom: 16,
                      left: 16,
                      child: GestureDetector(
                        onTap: () {
                          setState(() => _isCruiseHovered = !_isCruiseHovered);
                        },
                        child: MouseRegion(
                          onEnter: (_) =>
                              setState(() => _isCruiseHovered = true),
                          onExit: (_) =>
                              setState(() => _isCruiseHovered = false),
                          child: AnimatedContainer(
                            duration: const Duration(milliseconds: 300),
                            curve: Curves.easeInOut,
                            padding: EdgeInsets.symmetric(
                              horizontal: _isCruiseHovered ? 16 : 24,
                              vertical: _isCruiseHovered ? 12 : 16,
                            ),
                            decoration: BoxDecoration(
                              color: colorScheme.surfaceContainerHighest,
                              borderRadius: BorderRadius.circular(16),
                              boxShadow: [
                                BoxShadow(
                                  color: Colors.black.withAlpha(40),
                                  blurRadius: _isCruiseHovered ? 12 : 8,
                                  offset: const Offset(0, 4),
                                ),
                              ],
                            ),
                            child: Row(
                              mainAxisSize: MainAxisSize.min,
                              children: [
                                AnimatedSize(
                                  duration: const Duration(milliseconds: 300),
                                  curve: Curves.easeInOut,
                                  alignment: Alignment.centerLeft,
                                  child: _isCruiseHovered
                                      ? Row(
                                          mainAxisSize: MainAxisSize.min,
                                          children: [
                                            ElevatedButton(
                                              onPressed: cruiseSpeed > 0
                                                  ? () {
                                                      final newSpeed =
                                                          (cruiseSpeed - 1)
                                                              .clamp(0, 25);
                                                      setState(() {
                                                        _uart.cruiseControlTarget =
                                                            newSpeed.toDouble();
                                                      });
                                                      unawaited(
                                                        _uart.sendMsg(
                                                          'SET',
                                                          'CRUISE_CONTROL_TARGET',
                                                          newSpeed.toString(),
                                                        ),
                                                      );
                                                    }
                                                  : null,
                                              style: ElevatedButton.styleFrom(
                                                fixedSize: const Size(56, 56),
                                                padding: EdgeInsets.zero,
                                                shape: RoundedRectangleBorder(
                                                  borderRadius:
                                                      BorderRadius.circular(8),
                                                ),
                                              ),
                                              child: const Icon(
                                                Icons.remove,
                                                size: 32,
                                              ),
                                            ),
                                            const SizedBox(width: 16),
                                          ],
                                        )
                                      : const SizedBox(width: 0, height: 56),
                                ),
                                Column(
                                  mainAxisSize: MainAxisSize.min,
                                  children: [
                                    Icon(
                                      Icons.speed_rounded,
                                      size: 32,
                                      color: isCruiseControl
                                          ? colorScheme.primary
                                          : colorScheme.onSurface.withAlpha(
                                              100,
                                            ),
                                    ),
                                    if (_isCruiseHovered ||
                                        isCruiseControl) ...[
                                      const SizedBox(height: 4),
                                      GestureDetector(
                                        onTap: _showNumberPad,
                                        child: Text(
                                          '$cruiseSpeed',
                                          style: Theme.of(context)
                                              .textTheme
                                              .displaySmall
                                              ?.copyWith(
                                                color: colorScheme.primary,
                                                fontWeight: FontWeight.bold,
                                                fontSize: 36,
                                                height: 1.0,
                                              ),
                                        ),
                                      ),
                                      const SizedBox(height: 4),
                                      Text(
                                        'MPH',
                                        style: Theme.of(context)
                                            .textTheme
                                            .labelMedium
                                            ?.copyWith(
                                              color: colorScheme.onSurface,
                                              fontWeight: FontWeight.w600,
                                            ),
                                      ),
                                    ],
                                  ],
                                ),
                                AnimatedSize(
                                  duration: const Duration(milliseconds: 300),
                                  curve: Curves.easeInOut,
                                  alignment: Alignment.centerRight,
                                  child: _isCruiseHovered
                                      ? Row(
                                          mainAxisSize: MainAxisSize.min,
                                          children: [
                                            const SizedBox(width: 16),
                                            ElevatedButton(
                                              onPressed: cruiseSpeed < 25
                                                  ? () {
                                                      final newSpeed =
                                                          (cruiseSpeed + 1)
                                                              .clamp(0, 25);
                                                      setState(() {
                                                        _uart.cruiseControlTarget =
                                                            newSpeed.toDouble();
                                                      });
                                                      unawaited(
                                                        _uart.sendMsg(
                                                          'SET',
                                                          'CRUISE_CONTROL_TARGET',
                                                          newSpeed.toString(),
                                                        ),
                                                      );
                                                    }
                                                  : null,
                                              style: ElevatedButton.styleFrom(
                                                fixedSize: const Size(56, 56),
                                                padding: EdgeInsets.zero,
                                                shape: RoundedRectangleBorder(
                                                  borderRadius:
                                                      BorderRadius.circular(8),
                                                ),
                                              ),
                                              child: const Icon(
                                                Icons.add,
                                                size: 32,
                                              ),
                                            ),
                                            const SizedBox(width: 16),
                                            ElevatedButton(
                                              onPressed: () {
                                                final bool newEnabled =
                                                    !isCruiseControl;
                                                final int target = newEnabled
                                                    ? (cruiseSpeed == 0
                                                          ? 5
                                                          : cruiseSpeed)
                                                    : 0;
                                                setState(() {
                                                  _uart.cruiseControlTarget =
                                                      target.toDouble();
                                                });
                                                unawaited(
                                                  _uart.sendMsg(
                                                    'SET',
                                                    'CRUISE_CONTROL_TARGET',
                                                    target.toString(),
                                                  ),
                                                );
                                              },
                                              style: ElevatedButton.styleFrom(
                                                fixedSize: const Size(56, 56),
                                                padding: EdgeInsets.zero,
                                                backgroundColor: isCruiseControl
                                                    ? colorScheme.primary
                                                    : null,
                                                shape: RoundedRectangleBorder(
                                                  borderRadius:
                                                      BorderRadius.circular(8),
                                                ),
                                              ),
                                              child: Icon(
                                                Icons.power_settings_new,
                                                size: 32,
                                                color: isCruiseControl
                                                    ? colorScheme.onPrimary
                                                    : null,
                                              ),
                                            ),
                                          ],
                                        )
                                      : const SizedBox(width: 0, height: 56),
                                ),
                              ],
                            ),
                          ),
                        ),
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
