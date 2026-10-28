/*
TODO:
- go back/finish
*/

#define DEFAULT_BUTTON_DEBOUNCE 2000

#define CUSTOM_DEFINITIONS
#ifdef CUSTOM_DEFINITIONS
	#define LOW 0
	#define HIGH 1
	#define INPUT 0
	#define OUTPUT 1
	typedef unsigned char byte;
	void pinMode(byte, int);
	byte digitalRead(byte);
	void digitalWrite(byte, byte);
	void analogWrite(byte, byte);
	void delay(int);
	unsigned long millis();
	float constrain(float, float, float);
	float fabs(float);
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

namespace Speed
{
	constexpr byte min = 0;
	constexpr byte max = 255;
	constexpr byte low = max*1/5;
	constexpr byte medium = max*2/5;
	constexpr byte fast = max*3/5;

	constexpr byte fromNormalized(float value) {
		return constrain(max * value, min, max);
	}
}

namespace Direcao {
	enum DirecaoX { Reto, Esquerda, Direita };
	enum DirecaoY { Parado, Frente, Tras };
}


struct Motor {
private:
	pin pinoNeg_;
	pin pinoPos_;
	pin pinoHabilitador_;

	byte speed_ = 0;
	Direcao::DirecaoY direcao_ = Direcao::Parado;

public:
	Motor(pin pinoNeg, pin pinoPos, pin pinoHabilitador) {
		pinoNeg_ = pinoNeg;
		pinoPos_ = pinoPos;
		pinoHabilitador_ = pinoHabilitador;
	}

	void setup(byte speed = 0) {
		pinMode(pinoNeg_, OUTPUT);
		pinMode(pinoPos_, OUTPUT);
		pinMode(pinoHabilitador_, OUTPUT);
		setSpeed(speed);
	}

	byte getSpeed() { return speed_; }
	void setSpeed(byte speed) {
		speed_ = speed;
		analogWrite(pinoHabilitador_, speed_);
	}

	void go(Direcao::DirecaoY direcaoY) {
		switch (direcaoY)
		{
			case Direcao::Parado:
				digitalWrite(pinoNeg_, LOW);
				digitalWrite(pinoPos_, LOW);
				break;
			case Direcao::Frente:
				digitalWrite(pinoNeg_, LOW);
				digitalWrite(pinoPos_, HIGH);
				break;
			case Direcao::Tras:
				digitalWrite(pinoNeg_, HIGH);
				digitalWrite(pinoPos_, LOW);
				break;
		}
	}

	void stop() { go(Direcao::Parado); }
	void goForward() { go(Direcao::Frente); }
	void goBack() { go(Direcao::Tras); }
};

class Carro {
public:
	Motor motorEsquerdo;
	Motor motorDireito;

	Carro(Motor esquerdo, Motor direito): motorEsquerdo(esquerdo), motorDireito(direito) {}

	void setup(byte speed = 0) {
		motorEsquerdo.setup(speed);
		motorDireito.setup(speed);
		goForward();
	}

	void setSpeed(byte speed = 0) {
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

	void turn(Direcao::DirecaoX direcao, bool soft = false) {
		switch (direcao)
		{
			case Direcao::Reto:
				goForward();
				break;
			case Direcao::Esquerda:
				motorEsquerdo.stop();
				motorDireito.goForward();
				motorDireito.setSpeed(soft ? Speed::medium : Speed::max);
				break;
			case Direcao::Direita:
				motorEsquerdo.goForward();
				motorDireito.stop();
				motorEsquerdo.setSpeed(soft ? Speed::medium : Speed::max);
				break;
		}
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

class Sensor {
private:
	pin pin_;
public:
	Sensor(pin pin): pin_(pin) {}

	void setup() {
		pinMode(pin_, INPUT);
	}

	byte read() { return digitalRead(pin_); }
	bool isOn() { return read(); }
};

struct PathScanner {
	Sensor sensorEsquerdo;
	Sensor sensorMeio;
	Sensor sensorDireito;
	unsigned int esquerdo : 1;
	unsigned int meio : 1;
	unsigned int direito : 1;

	PathScanner(
		Sensor sEsquerdo,
		Sensor sMeio,
		Sensor sDireito
	): sensorEsquerdo(sEsquerdo), sensorMeio(sMeio), sensorDireito(sDireito) {}

	void setup() {
		sensorEsquerdo.setup();
		sensorMeio.setup();
		sensorDireito.setup();
	}
	
	void update() {
		esquerdo = sensorEsquerdo.isOn() ? 1 : 0;
		meio = sensorMeio.isOn() ? 1 : 0;
		direito = sensorDireito.isOn() ? 1 : 0;
	}

	float getRatio() {
		float guy = 0;
		if (esquerdo) guy--;
		if (direito) guy++;
		if (meio) guy /= 2.0;

		return guy;
	}


	bool isPathVisible() { return esquerdo || meio || direito; }
	bool shouldTurnLeft() { return esquerdo && !direito; }
	bool shouldTurnRight() { return direito && !esquerdo; }
	bool shouldGoStraight() {
		return (
			(!esquerdo && meio && !direito)
			|| (esquerdo && meio && direito)
			|| (esquerdo && !meio && direito)
		);
	}

	Direcao::DirecaoX getRecommendedPath() {
		if (!isPathVisible() || shouldGoStraight()) return Direcao::Reto;
		if (shouldTurnLeft()) return Direcao::Esquerda;
		if (shouldTurnRight()) return Direcao::Direita;
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


bool isStarted;
unsigned long currentTime;
unsigned long delta;
long debounce = DEFAULT_BUTTON_DEBOUNCE;

unsigned long calculateDelta() {
	auto oldTime = currentTime;
	currentTime = millis();
	return currentTime - oldTime;
}

void setup() {
	carro.setup(Speed::fast);
	scannerCaminho.setup();

	pinMode(Pinos::botao, INPUT);
	isStarted = false;
}

void loop() {
	delta = calculateDelta();
	scannerCaminho.update();

	debounce -= debounce > 0 ? delta : 0;

	if (digitalRead(Pinos::botao)) {
		if (debounce <= 0) {
			isStarted = !isStarted;
			debounce = DEFAULT_BUTTON_DEBOUNCE;
		}
	}

	if (isStarted) {
		auto ratio = scannerCaminho.getRatio();
		carro.turnFromRatio(ratio);
		if (!ratio) {
			if (!scannerCaminho.isPathVisible()) {
				// TODO: make it go back and then go foward and then stop to be sure
				carro.goBack();
			} else {
				carro.setSpeed(Speed::max);
				carro.goForward();
			}
		}
	} else {
		carro.setSpeed(Speed::min);	
	}
}
