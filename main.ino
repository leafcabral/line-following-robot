//#define DEBUG


typedef byte pin;


namespace Pinos {
	namespace Motores {
		constexpr pin esquerdoNegativo = 4;
		constexpr pin esquerdoPositivo = 2;
		constexpr pin esquerdoHabilitador = 3;
		constexpr pin direitoNegativo = 7;
		constexpr pin direitoPositivo = 5;
		constexpr pin direitoHabilitador = 6;
	}

	namespace Sensores {
		constexpr pin esquerdo = 10;
		constexpr pin meio = 11;
		constexpr pin direito = 12;
	}

	constexpr pin botao = 8;
}

namespace Speed {
	constexpr byte zero = 0;
	constexpr byte min = 40;
	constexpr byte max = 60;

	constexpr byte fromNormalized(float value) {
		return constrain(max * value, min, max);
	}
}

namespace Timing {
	constexpr unsigned long buttonDebounce = 1000;
	constexpr unsigned long pathLostContinueDuration = 1500;
}

namespace Direcao {
	enum DirecaoY { Parado, Frente, Tras };
}

namespace Program {
	enum State {
		Stopped,
		FollowingLine,
		NoLineFoundOhNo
	};
}

struct Motor {
	constexpr Motor(pin pinoNeg, pin pinoPos, pin pinoHabilitador)
		: pinoNeg_(pinoNeg), pinoPos_(pinoPos), pinoHabilitador_(pinoHabilitador) {}

	void setup(byte speed) {
		pinMode(pinoNeg_, OUTPUT);
		pinMode(pinoPos_, OUTPUT);
		pinMode(pinoHabilitador_, OUTPUT);
		setSpeed(speed);
	}

	void setSpeed(byte speed) { analogWrite(pinoHabilitador_, speed); }

	void go(Direcao::DirecaoY direcao) {
		if (direcao == Direcao::Parado) write_(LOW, LOW);
		else if (direcao == Direcao::Frente) write_(LOW, HIGH);
		else if (direcao == Direcao::Tras) write_(HIGH, LOW);
	}

	void stop() { go(Direcao::Parado); }
	void goForward() { go(Direcao::Frente); }
	void goBack() { go(Direcao::Tras); }

private:
	pin pinoNeg_;
	pin pinoPos_;
	pin pinoHabilitador_;
	
	inline void write_(byte neg, byte pos) {
		digitalWrite(pinoNeg_, neg);
		digitalWrite(pinoPos_, pos);
	}
};

struct Carro {
	Motor motorEsquerdo;
	Motor motorDireito;

	constexpr Carro(Motor esquerdo, Motor direito)
		: motorEsquerdo(esquerdo), motorDireito(direito) {}

	void setup(byte speed) {
		motorEsquerdo.setup(speed);
		motorDireito.setup(speed);
		stop();
	}

	void setSpeed(byte speed) {
		motorEsquerdo.setSpeed(speed);
		motorDireito.setSpeed(speed);
	}

	void stop() {
		motorEsquerdo.stop();
		motorDireito.stop();
	}

	void goForward() {
		motorEsquerdo.goForward();
		motorDireito.goForward();
	}
  
	void goBack() {
		motorEsquerdo.goBack();
		motorDireito.goBack();
	}
  
	void spinRight() {
		motorEsquerdo.goForward();
		motorDireito.goBack();
	}

	void spinLeft() {
		motorEsquerdo.goBack();
		motorDireito.goForward();
	}

	void turnFromRatio(float ratio) {
		auto speed = Speed::fromNormalized(fabs(ratio));
		
		#ifdef DEBUG
			Serial.print(ratio);
			Serial.print(" ");
			Serial.print(speed);
			Serial.print(" ");
		#endif

		if (ratio > 0) {
			motorEsquerdo.goForward();
			motorEsquerdo.setSpeed(speed);

			motorDireito.goBack();
			motorDireito.setSpeed(speed);
		} else if (ratio < 0) {
			motorDireito.goForward();
			motorDireito.setSpeed(speed);

			motorEsquerdo.goBack();
			motorEsquerdo.setSpeed(speed);
		}
	}
};

struct Sensor {
	constexpr Sensor(pin pin): pin_(pin) {}

	void setup() { pinMode(pin_, INPUT); }

	bool isOn() { return !digitalRead(pin_); }
	
private:
	pin pin_;
};

struct PathScanner {
	Sensor sensorEsquerdo;
	Sensor sensorMeio;
	Sensor sensorDireito;

	constexpr PathScanner(
		Sensor sEsquerdo,
		Sensor sMeio,
		Sensor sDireito)
	: sensorEsquerdo(sEsquerdo), sensorMeio(sMeio), sensorDireito(sDireito) {}

	void setup() {
		sensorEsquerdo.setup();
		sensorMeio.setup();
		sensorDireito.setup();
	}
	
	float getRatio() {
		return (sensorDireito.isOn() - sensorEsquerdo.isOn())
			* (sensorMeio.isOn() ? 0.5f : 1.0f);
	}
	bool isPathVisible() {
		return sensorDireito.isOn() || sensorEsquerdo.isOn() || sensorMeio.isOn();
	}
};


Carro carro = Carro(
	Motor{
		Pinos::Motores::esquerdoNegativo,
		Pinos::Motores::esquerdoPositivo,
		Pinos::Motores::esquerdoHabilitador
	},
	Motor{
		Pinos::Motores::direitoNegativo,
		Pinos::Motores::direitoPositivo,
		Pinos::Motores::direitoHabilitador
	}
);

PathScanner scannerCaminho = PathScanner(
	Sensor{Pinos::Sensores::esquerdo},
	Sensor{Pinos::Sensores::meio},
	Sensor{Pinos::Sensores::direito}
);

Program::State programState = Program::Stopped;
unsigned long currentTime;
unsigned long stateStartedAt;
long delta;
long debounce = Timing::buttonDebounce;
float ratio = 0;

inline void updateDelta() {
	auto oldTime = currentTime;
	currentTime = millis();
	delta =  currentTime - oldTime;
}

inline void preloop() {
	updateDelta();
	debounce = max(debounce - delta, 0);
}

inline void updateState(Program::State newState) {
	programState = newState;
	stateStartedAt = currentTime;

	if (newState == Program::Stopped) { carro.stop(); }
}

inline void handleButton() {
	if (digitalRead(Pinos::botao)) return;

	debounce = Timing::buttonDebounce;

	if (programState == Program::Stopped) {
		updateState(Program::FollowingLine);
	} else {
		updateState(Program::Stopped);
	}
}

void setup() {
	carro.setup(Speed::max);
	scannerCaminho.setup();
	pinMode(Pinos::botao, INPUT_PULLUP);
  
	#ifdef DEBUG
		Serial.begin(9600);
	#endif

	currentTime = millis();
	updateState(Program::Stopped);
}

void loop() {
	preloop();

	#ifndef DEBUG
		if (!debounce) {
			handleButton();
		}
	#else
		updateState(Program::FollowingLine);
	#endif

	switch (programState) {
		case Program::FollowingLine: {
			ratio = scannerCaminho.getRatio();
          
			if (!scannerCaminho.isPathVisible()) {
				updateState(Program::NoLineFoundOhNo);
				break;
			}
			if (!ratio) {
				carro.setSpeed(Speed::max);
				carro.goForward();
			} else {
				carro.turnFromRatio(ratio);
			}
			break;
		}

		case Program::NoLineFoundOhNo:
			if (scannerCaminho.isPathVisible()) {
				updateState(Program::FollowingLine);
			}
			else if (currentTime - stateStartedAt >= Timing::pathLostContinueDuration) {
				updateState(Program::Stopped);
				break;
			}

			carro.setSpeed(Speed::fromNormalized(0.5));
			carro.goBack();

			break;
			// carro.turnFromRatio(ratio < 0 ? -1 : 1);
	}
}
