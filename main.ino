#define ARUDINO_DEF_FIX
#ifdef ARUDINO_DEF_FIX
	#define LOW 0
	#define HIGH 1
	#define INPUT 0
	#define OUTPUT 1
	typedef unsigned char byte;
	void pinMode(byte, int);
	byte digitalRead(byte);
	void digitalWrite(byte, byte);
	void analogWrite(byte, byte);
	unsigned long millis();
	float constrain(float, float, float);
	float fabs(float);
	float max(float, float);
#endif


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
	constexpr byte min = 0;
	constexpr byte max = 255;

	constexpr byte fromNormalized(float value) {
		return constrain(max * value, min, max);
	}
}

namespace Timing {
	constexpr unsigned long buttonDebounce = 1000;
	constexpr unsigned long pathLostContinueDuration = 500;
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

	void spinRight() {
		motorEsquerdo.goForward();
		motorDireito.goBack();
	}

	void spinLeft() {
		motorEsquerdo.goBack();
		motorDireito.goForward();
	}

	void turnFromRatio(float ratio) {
		if (ratio > 0) {
			motorDireito.stop();
			motorEsquerdo.goForward();
			motorEsquerdo.setSpeed(Speed::fromNormalized(fabs(ratio)));
		} else if (ratio < 0) {
			motorEsquerdo.stop();
			motorDireito.goForward();
			motorDireito.setSpeed(Speed::fromNormalized(fabs(ratio)));
		}
	}
};

struct Sensor {
	constexpr Sensor(pin pin): pin_(pin) {}

	void setup() { pinMode(pin_, INPUT); }

	bool isOn() { return digitalRead(pin_); }
	
private:
	pin pin_;
};

struct PathScanner {
	Sensor sensorEsquerdo;
	Sensor sensorMeio;
	Sensor sensorDireito;
	unsigned int esquerdo : 1;
	unsigned int meio : 1;
	unsigned int direito : 1;

	constexpr PathScanner(
		Sensor sEsquerdo,
		Sensor sMeio,
		Sensor sDireito)
	: sensorEsquerdo(sEsquerdo), sensorMeio(sMeio), sensorDireito(sDireito), esquerdo(0), meio(0), direito(0) {}

	void setup() {
		sensorEsquerdo.setup();
		sensorMeio.setup();
		sensorDireito.setup();
	}
	
	void update() {
		esquerdo = sensorEsquerdo.isOn();
		meio = sensorMeio.isOn();
		direito = sensorDireito.isOn();
	}

	float getRatio() { return (direito - esquerdo) * (meio ? 0.5f : 1.0f); }
	bool isPathVisible() { return esquerdo || meio || direito; }
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
unsigned long debounce = Timing::buttonDebounce;

inline void updateDelta() {
	auto oldTime = currentTime;
	currentTime = millis();
	delta =  (long)currentTime - (long)oldTime;
}

inline void preloop() {
	updateDelta();
	debounce = max((long)debounce - (long)delta, 0);
	scannerCaminho.update();
}

inline void updateState(Program::State newState) {
	programState = newState;
	stateStartedAt = currentTime;

	if (newState == Program::Stopped) { carro.stop(); }
}

inline void handleButton() {
	if (!digitalRead(Pinos::botao)) return;

	debounce = Timing::buttonDebounce;

	if (programState == Program::Stopped) {
		updateState(Program::FollowingLine);
	} else {
		updateState(Program::Stopped);
		carro.stop();
	}
}

void setup() {
	carro.setup(Speed::max);
	scannerCaminho.setup();
	pinMode(Pinos::botao, INPUT);

	currentTime = millis();
	updateState(Program::Stopped);
}

void loop() {
	preloop();

	if (!debounce) {
		handleButton();
	}

	switch (programState) {
		case Program::FollowingLine:
			if (!scannerCaminho.isPathVisible()) {
				carro.setSpeed(Speed::max);
				carro.goForward();

				updateState(Program::NoLineFoundOhNo);
				break;
			}

			{
				auto ratio = scannerCaminho.getRatio();
				carro.turnFromRatio(ratio);
				if (!ratio) {
					carro.setSpeed(Speed::max);
					carro.goForward();
				}
			}
			break;

		case Program::NoLineFoundOhNo:
			if (currentTime - stateStartedAt >= Timing::pathLostContinueDuration) {
				updateState(Program::Stopped);
			}
			break;
	}
}