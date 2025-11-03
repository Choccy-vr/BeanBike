/*import 'dart:async';
import 'package:dart_periphery/dart_periphery.dart';

class UARTService {
  UARTService({
    required String devicePath,
    Baudrate baudrate = Baudrate.b115200,
    Duration pollInterval = const Duration(milliseconds: 50),
  }) : _devicePath = devicePath,
       _baudrate = baudrate,
       _pollInterval = pollInterval;

  final String _devicePath;
  final Baudrate _baudrate;
  final Duration _pollInterval;

  Serial? _serial;
  Timer? _pollTimer;
  final _rxController = StreamController<String>.broadcast();
  final _frameController = StreamController<UARTFrame>.broadcast();
  StringBuffer _readBuffer = StringBuffer();
  static final RegExp _motorPwmRegExp = RegExp(
    r'A:\s*(-?\d+(?:\.\d+)?)\s+B:\s*(-?\d+(?:\.\d+)?)\s+C:\s*(-?\d+(?:\.\d+)?)',
  );

  bool get isOpen => _serial != null;
  Stream<String> get messages => _rxController.stream;
  Stream<UARTFrame> get frames => _frameController.stream;
  // Variables
  bool? motorPowerLimitActive;
  double? motorBusVolt;
  double? motorPhaseCurrent;
  int? motorPowerLimitPWM;
  int? drvCsaSenlvl;
  String? drvCalMode;
  double? drvPwmRequest;
  double? motorPowerW;
  double? motorRPM;
  bool? pasPedalActive;
  int? pasPWM;
  double? pasCadenceRPM;
  int? pasLevel;
  double? pasAssistRatio;
  double? pasTargetRPM;
  int? pasPWMRequest;
  int? pasModeEnabled;
  double? cruiseControlTarget;
  int? cruisePWMRequest;
  int? cruisePwm;
  double? speedMph;
  double? throttleVolt;
  double? throttleRatio;
  int? throttlePwm;
  bool? drv8353Init;
  bool? faultStatus;
  String? fault1;
  String? fault2;
  double? motorPWMA;
  double? motorPWMB;
  double? motorPWMC;
  bool? drvOcpAct;
  bool? drvGduvFault;
  bool? drvGdfFault;
  bool? drvOtwReport;
  String? drvPwmMode;
  bool? drv1PwmCom;
  bool? drv1PwmDir;
  String? motorMode;
  bool? drvLock;
  int? drvHsIdrivep;
  int? drvHsIdriven;
  bool? drvLsCbc;
  int? drvLsTdrive;
  int? drvLsIdrivep;
  int? drvLsIdriven;
  String? drvOcpRetry;
  int? drvDeadtime;
  int? drvOcpMode;
  int? drvOcpDeg;
  int? drvVdsLvl;
  String? drvCsaFet;
  String? drvCsaVref;
  String? drvCsaLsref;
  int? drvCsaGain;
  bool? drvCsaOcp;
  bool? drvCsaCalA;
  bool? drvCsaCalB;
  bool? drvCsaCalC;
  // End of Variables
  Future<void> open() async {
    if (isOpen) {
      return;
    }
    _serial = Serial(_devicePath, _baudrate);
    _pollTimer = Timer.periodic(_pollInterval, (_) => _pollSerial());
  }

  Future<void> write(String message, {bool appendNewline = true}) async {
    final serial = _serial;
    if (serial == null) {
      throw StateError('Serial port is not open');
    }
    final payload = appendNewline ? '$message\r\n' : message;
    serial.writeString(payload);
  }

  void _pollSerial() {
    final serial = _serial;
    if (serial == null) {
      return;
    }

    try {
      final event = serial.read(256, _pollInterval.inMilliseconds);
      final data = event.toString();
      if (data.isEmpty) {
        return;
      }

      _readBuffer.write(data);
      var buffer = _readBuffer.toString();
      var newlineIndex = buffer.indexOf('\n');

      while (newlineIndex != -1) {
        final rawLine = buffer.substring(0, newlineIndex).replaceAll('\r', '');
        final line = rawLine.trim();

        if (line.isNotEmpty) {
          _rxController.add(line);
          final frame = UARTFrame.tryParse(line);
          if (frame != null) {
            _frameController.add(frame);
          }
        }

        buffer = buffer.substring(newlineIndex + 1);
        newlineIndex = buffer.indexOf('\n');
      }

      _readBuffer = StringBuffer()..write(buffer);
    } catch (error, stackTrace) {
      _rxController.addError(error, stackTrace);
      _frameController.addError(error, stackTrace);
    }
  }

  Future<void> sendMsg(String cmd, String item, String arg) async {
    final normalizedCmd = cmd.trim();
    final normalizedItem = item.trim();
    final normalizedArg = arg.trim();

    if (normalizedCmd.isEmpty) {
      throw ArgumentError('Command must not be empty');
    }
    if (normalizedItem.isEmpty) {
      throw ArgumentError('Item must not be empty');
    }

    final serial = _serial;
    if (serial == null) {
      throw StateError('Serial port is not open');
    }

    final payloadParts = <String>[normalizedCmd, normalizedItem];
    if (normalizedArg.isNotEmpty) {
      payloadParts.add(normalizedArg);
    }

    final payload = '${payloadParts.join(' ')}\r\n';
    serial.writeString(payload);
  }

  Future<void> parseMsg(UARTFrame frame) async {
    if (frame.command == '' || frame.command != "READ") {
      throw ArgumentError(
        'Command must not be empty and must be READ to parse',
      );
    } else if (frame.item == '') {
      throw ArgumentError('Item must not be empty');
    } else if (frame.argument == '') {
      throw ArgumentError('Argument must not be empty');
    }

    switch (frame.item) {
      //Motor
      case 'MOTOR_POWER_LIMIT_ACTIVE':
        motorPowerLimitActive = bool.tryParse(frame.argument);
        if (motorPowerLimitActive == null) {
          throw ArgumentError(
            'Invalid motorPowerLimitActive value: ${frame.argument}',
          );
        }
        break;
      case 'MOTOR_BUS_VOLT':
        motorBusVolt = double.tryParse(frame.argument);
        if (motorBusVolt == null) {
          throw ArgumentError('Invalid motorBusVolt value: ${frame.argument}');
        }
        break;
      case 'MOTOR_PHASE_CURRENT':
        motorPhaseCurrent = double.tryParse(frame.argument);
        if (motorPhaseCurrent == null) {
          throw ArgumentError(
            'Invalid motorPhaseCurrent value: ${frame.argument}',
          );
        }
        break;
      case 'MOTOR_POWER_W':
        motorPowerW = double.tryParse(frame.argument);
        if (motorPowerW == null) {
          throw ArgumentError('Invalid motorPowerW value: ${frame.argument}');
        }
      case 'MOTOR_POWER_LIMIT_PWM':
        motorPowerLimitPWM = int.tryParse(frame.argument);
        if (motorPowerLimitPWM == null) {
          throw ArgumentError(
            'Invalid motorPowerLimitPWM value: ${frame.argument}',
          );
        }
        break;
      case 'MOTOR_RPM':
        motorRPM = double.tryParse(frame.argument);
        if (motorRPM == null) {
          throw ArgumentError('Invalid motorRPM value: ${frame.argument}');
        }
        break;
      //PAS
      case 'PAS_PEDAL_ACTIVE':
        pasPedalActive = bool.tryParse(frame.argument);
        if (pasPedalActive == null) {
          throw ArgumentError(
            'Invalid pasPedalActive value: ${frame.argument}',
          );
        }
        break;
      case 'PAS_PWM':
        pasPWM = int.tryParse(frame.argument);
        if (pasPWM == null) {
          throw ArgumentError('Invalid pasPWM value: ${frame.argument}');
        }
        break;
      case 'PAS_CADENCE_RPM':
        pasCadenceRPM = double.tryParse(frame.argument);
        if (pasCadenceRPM == null) {
          throw ArgumentError('Invalid pasCadenceRPM value: ${frame.argument}');
        }
        break;
      case 'PAS_LEVEL':
        pasLevel = int.tryParse(frame.argument);
        if (pasLevel == null) {
          throw ArgumentError('Invalid pasLevel value: ${frame.argument}');
        }
        break;
      case 'PAS_ASSIST_RATIO':
        pasAssistRatio = double.tryParse(frame.argument);
        if (pasAssistRatio == null) {
          throw ArgumentError(
            'Invalid pasAssistRatio value: ${frame.argument}',
          );
        }
        break;
      case 'PAS_TARGET_RPM':
        pasTargetRPM = double.tryParse(frame.argument);
        if (pasTargetRPM == null) {
          throw ArgumentError('Invalid pasTargetRPM value: ${frame.argument}');
        }
        break;
      case 'PAS_PWM_REQUEST':
        pasPWMRequest = int.tryParse(frame.argument);
        if (pasPWMRequest == null) {
          throw ArgumentError('Invalid pasPWMRequest value: ${frame.argument}');
        }
        break;
      case 'PAS_MODE_ENABLED':
        pasModeEnabled = int.tryParse(frame.argument);
        if (pasModeEnabled == null) {
          throw ArgumentError(
            'Invalid pasModeEnabled value: ${frame.argument}',
          );
        }
        break;
      //Cruise Control
      case 'CRUISE_CONTROL_TARGET':
        cruiseControlTarget = double.tryParse(frame.argument);
        if (cruiseControlTarget == null) {
          throw ArgumentError(
            'Invalid cruiseControlTarget value: ${frame.argument}',
          );
        }
        break;
      case 'CRUISE_PWM_REQUEST':
        cruisePWMRequest = int.tryParse(frame.argument);
        if (cruisePWMRequest == null) {
          throw ArgumentError(
            'Invalid cruisePWMRequest value: ${frame.argument}',
          );
        }
        break;
      case 'CRUISE_PWM':
        cruisePwm = int.tryParse(frame.argument);
        if (cruisePwm == null) {
          throw ArgumentError('Invalid cruisePwm value: ${frame.argument}');
        }
        break;
      case 'MOTOR_SPEED_MPH':
        speedMph = double.tryParse(frame.argument);
        if (speedMph == null) {
          throw ArgumentError('Invalid speed value: ${frame.argument}');
        }
        break;
      //Throttle
      case 'THROTTLE_VOLT':
        throttleVolt = double.tryParse(frame.argument);
        if (throttleVolt == null) {
          throw ArgumentError('Invalid throttleVolt value: ${frame.argument}');
        }
        // Handle speed value
        break;
      case 'THROTTLE_RATIO':
        throttleRatio = double.tryParse(frame.argument);
        if (throttleRatio == null) {
          throw ArgumentError('Invalid throttleRatio value: ${frame.argument}');
        }
        break;
      case 'THROTTLE_PWM':
        throttlePwm = int.tryParse(frame.argument);
        if (throttlePwm == null) {
          throw ArgumentError('Invalid throttlePwm value: ${frame.argument}');
        }
        break;
      //DRV8353
      case 'DRV8353_INITIALIZE':
        drv8353Init = bool.tryParse(frame.argument);
        if (drv8353Init == null) {
          throw ArgumentError('Invalid drv8353Init value: ${frame.argument}');
        }
        break;
      case 'FAULT_STATUS':
        faultStatus = bool.tryParse(frame.argument);
        if (faultStatus == null) {
          throw ArgumentError('Invalid faultStatus value: ${frame.argument}');
        }
        break;
      case 'FAULT1':
        fault1 = frame.argument;
        if (fault1 == null) {
          throw ArgumentError('Invalid fault1 value: ${frame.argument}');
        }
        break;
      case 'FAULT2':
        fault2 = frame.argument;
        if (fault2 == null) {
          throw ArgumentError('Invalid fault2 value: ${frame.argument}');
        }
        break;
      case 'MOTOR_PWM':
        final match = _motorPwmRegExp.firstMatch(frame.argument);
        if (match == null) {
          throw ArgumentError('Invalid MOTOR_PWM value: ${frame.argument}');
        }
        motorPWMA = double.parse(match.group(1)!);
        motorPWMB = double.parse(match.group(2)!);
        motorPWMC = double.parse(match.group(3)!);
        break;
      case 'CLEAR_FAULT':
        fault1 = '';
        fault2 = '';
        break;
      case 'DRV_OCP_ACT':
        drvOcpAct = bool.tryParse(frame.argument);
        if (drvOcpAct == null) {
          throw ArgumentError('Invalid drvOcpAct value: ${frame.argument}');
        }
        break;
      case 'DRV_GDUV_FAULT':
        drvGduvFault = bool.tryParse(frame.argument);
        if (drvGduvFault == null) {
          throw ArgumentError('Invalid drvGduvFault value: ${frame.argument}');
        }
        break;
      case 'DRV_GDF_FAULT':
        drvGdfFault = bool.tryParse(frame.argument);
        if (drvGdfFault == null) {
          throw ArgumentError('Invalid drvGdfFault value: ${frame.argument}');
        }
        break;
      case 'DRV_OTW_REPORT':
        drvOtwReport = bool.tryParse(frame.argument);
        if (drvOtwReport == null) {
          throw ArgumentError('Invalid drvOtwReport value: ${frame.argument}');
        }
        break;
      case 'DRV_PWM_MODE':
        drvPwmMode = frame.argument;
        break;
      case 'DRV_1PWM_COM':
        drv1PwmCom = bool.tryParse(frame.argument);
        if (drv1PwmCom == null) {
          throw ArgumentError('Invalid drv1PwmCom value: ${frame.argument}');
        }
        break;
      case 'DRV_1PWM_DIR':
        drv1PwmDir = bool.tryParse(frame.argument);
        if (drv1PwmDir == null) {
          throw ArgumentError('Invalid drv1PwmDir value: ${frame.argument}');
        }
        break;
      case 'MOTOR_MODE':
        motorMode = frame.argument;
        if (motorMode == null) {
          throw ArgumentError('Invalid motorMode value: ${frame.argument}');
        }

        // Handle battery value
        break;
      case 'DRV_LOCK':
        drvLock = bool.tryParse(frame.argument);
        if (drvLock == null) {
          throw ArgumentError('Invalid drvLock value: ${frame.argument}');
        }
        break;
      case 'DRV_HS_IDRIVEP':
        drvHsIdrivep = int.tryParse(frame.argument);
        if (drvHsIdrivep == null) {
          throw ArgumentError('Invalid drvHsIdrivep value: ${frame.argument}');
        }
        break;
      case 'DRV_HS_IDRIVEN':
        drvHsIdriven = int.tryParse(frame.argument);
        if (drvHsIdriven == null) {
          throw ArgumentError('Invalid drvHsIdriven value: ${frame.argument}');
        }
        break;
      case 'DRV_LS_CBC':
        drvLsCbc = bool.tryParse(frame.argument);
        if (drvLsCbc == null) {
          throw ArgumentError('Invalid drvLsCbc value: ${frame.argument}');
        }
        break;
      case 'DRV_LS_TDRIVE':
        drvLsTdrive = int.tryParse(frame.argument);
        if (drvLsTdrive == null) {
          throw ArgumentError('Invalid drvLsTdrive value: ${frame.argument}');
        }
        // Handle speed value
        break;
      case 'DRV_LS_IDRIVEP':
        drvLsIdrivep = int.tryParse(frame.argument);
        if (drvLsIdrivep == null) {
          throw ArgumentError('Invalid drvLsIdrivep value: ${frame.argument}');
        }
        break;
      case 'DRV_LS_IDRIVEN':
        drvLsIdriven = int.tryParse(frame.argument);
        if (drvLsIdriven == null) {
          throw ArgumentError('Invalid drvLsIdriven value: ${frame.argument}');
        }
        break;
      case 'DRV_OCP_RETRY':
        drvOcpRetry = frame.argument;
        if (drvOcpRetry == null) {
          throw ArgumentError('Invalid drvOcpRetry value: ${frame.argument}');
        }
        // Handle speed value
        break;
      case 'DRV_DEADTIME':
        drvDeadtime = int.tryParse(frame.argument);
        if (drvDeadtime == null) {
          throw ArgumentError('Invalid drvDeadtime value: ${frame.argument}');
        }
        break;
      case 'DRV_OCP_MODE':
        drvOcpMode = int.tryParse(frame.argument);
        if (drvOcpMode == null) {
          throw ArgumentError('Invalid drvOcpMode value: ${frame.argument}');
        }
        break;
      case 'DRV_OCP_DEG':
        drvOcpDeg = int.tryParse(frame.argument);
        if (drvOcpDeg == null) {
          throw ArgumentError('Invalid drvOcpDeg value: ${frame.argument}');
        }
        break;
      case 'DRV_VDS_LVL':
        drvVdsLvl = int.tryParse(frame.argument);
        if (drvVdsLvl == null) {
          throw ArgumentError('Invalid drvVdsLvl value: ${frame.argument}');
        }
        break;
      case 'DRV_CSA_FET':
        drvCsaFet = frame.argument;
        if (drvCsaFet == null) {
          throw ArgumentError('Invalid drvCsaFet value: ${frame.argument}');
        }
        break;
      case 'DRV_CSA_VREF':
        drvCsaVref = frame.argument;
        if (drvCsaVref == null) {
          throw ArgumentError('Invalid drvCsaVref value: ${frame.argument}');
        }
        break;
      case 'DRV_CSA_LSREF':
        drvCsaLsref = frame.argument;
        if (drvCsaLsref == null) {
          throw ArgumentError('Invalid drvCsaLsref value: ${frame.argument}');
        }
        break;
      case 'DRV_CSA_GAIN':
        drvCsaGain = int.tryParse(frame.argument);
        if (drvCsaGain == null) {
          throw ArgumentError('Invalid drvCsaGain value: ${frame.argument}');
        }
        break;
      case 'DRV_CSA_OCP':
        drvCsaOcp = bool.tryParse(frame.argument);
        if (drvCsaOcp == null) {
          throw ArgumentError('Invalid drvCsaOcp value: ${frame.argument}');
        }
        break;
      case 'DRV_CSA_CAL_A':
        drvCsaCalA = bool.tryParse(frame.argument);
        if (drvCsaCalA == null) {
          throw ArgumentError('Invalid drvCsaCalA value: ${frame.argument}');
        }
        break;
      case 'DRV_CSA_CAL_B':
        drvCsaCalB = bool.tryParse(frame.argument);
        if (drvCsaCalB == null) {
          throw ArgumentError('Invalid drvCsaCalB value: ${frame.argument}');
        }
        break;
      case 'DRV_CSA_CAL_C':
        drvCsaCalC = bool.tryParse(frame.argument);
        if (drvCsaCalC == null) {
          throw ArgumentError('Invalid drvCsaCalC value: ${frame.argument}');
        }
        break;
      case 'DRV_CSA_SENLVL':
        drvCsaSenlvl = int.tryParse(frame.argument);
        if (drvCsaSenlvl == null) {
          throw ArgumentError('Invalid drvCsaSenlvl value: ${frame.argument}');
        }
        break;
      case 'DRV_CAL_MODE':
        drvCalMode = frame.argument;
        if (drvCalMode == null) {
          throw ArgumentError('Invalid drvCalMode value: ${frame.argument}');
        }
        break;
    }
  }

  Future<void> close() async {
    _pollTimer?.cancel();
    _pollTimer = null;
    _serial?.dispose();
    _serial = null;
    _readBuffer = StringBuffer();
  }

  Future<void> dispose() async {
    await close();
    await _rxController.close();
    await _frameController.close();
  }
}

class UARTFrame {
  const UARTFrame({
    required this.command,
    required this.item,
    required this.argument,
  });

  final String command;
  final String item;
  final String argument;

  static UARTFrame? tryParse(String input) {
    final trimmed = input.trim();
    final firstSpace = trimmed.indexOf(' ');
    if (firstSpace < 0) {
      return null;
    }

    final cmd = trimmed.substring(0, firstSpace).trim();
    if (cmd.isEmpty) {
      return null;
    }

    final secondSpace = trimmed.indexOf(' ', firstSpace + 1);
    final String item;
    final String arg;

    if (secondSpace < 0) {
      item = trimmed.substring(firstSpace + 1).trim();
      arg = '';
    } else {
      item = trimmed.substring(firstSpace + 1, secondSpace).trim();
      arg = trimmed.substring(secondSpace + 1).trim();
    }

    if (item.isEmpty) {
      return null;
    }

    return UARTFrame(command: cmd, item: item, argument: arg);
  }
}
*/
