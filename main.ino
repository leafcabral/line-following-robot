/*
TODO:
- smooth direction
- go back/finish
*/

//#define DEBUG_MODE
#define CUSTOM_DEFINITIONS

#define DEFAULT_BUTTON_DEBOUNCE 2000

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
#endif

typedef byte pin;
bool isStarted;
unsigned long currentTime;
unsigned long delta;
long debounce = DEFAULT_BUTTON_DEBOUNCE;

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
}

namespace Direcao {
	enum DirecaoX { Reto, Esquerda, Direita };
	enum DirecaoY { Parado, Frente, Tras };
}

class Motor {
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

	void turn(Direcao::DirecaoX direcao) {
		switch (direcao)
		{
			case Direcao::Reto:
				goForward();
				break;
			case Direcao::Esquerda:
				motorEsquerdo.stop();
				motorDireito.goForward();
				break;
			case Direcao::Direita:
				motorEsquerdo.goForward();
				motorDireito.stop();
				break;
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

	bool isOn() {
		return digitalRead(pin_) == HIGH ? true : false;
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

struct Sensores {
	Sensor esquerdo, meio, direito;
} sensores = {
	.esquerdo = Sensor(Pinos::Sensores::esquerdo),
	.meio = Sensor(Pinos::Sensores::meio),
	.direito = Sensor(Pinos::Sensores::direito),
};

unsigned long calculateDelta() {
	auto oldTime = currentTime;
	currentTime = millis();
	return currentTime - oldTime;
}

void setup() {
	carro.setup(Speed::fast);

	sensores.esquerdo.setup();
	sensores.meio.setup();
	sensores.direito.setup();
	pinMode(Pinos::botao, INPUT);
	isStarted = false;
}

void loop() {
	delta = calculateDelta();

	debounce -= debounce > 0 ? delta : 0;

	if (digitalRead(Pinos::botao)) {
		if (debounce <= 0) {
			isStarted = !isStarted;
			debounce = DEFAULT_BUTTON_DEBOUNCE;
		}
	}

	if (isStarted) {
		carro.setSpeed(Speed::low);
	} else {
		carro.setSpeed(Speed::min);	
	}
	
	// if (sensores.direito.isOn()) {
	// 	carro.turn(Direcao::Direita);
	// } else if (sensores.esquerdo.isOn()) {
	// 	carro.turn(Direcao::Esquerda);
	// } else if (sensores.meio.isOn()) {
	// 	carro.goForward();
	// } else {
	// 	carro.goBack();
	// }
}
