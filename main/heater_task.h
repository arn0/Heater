struct heat_stat {
	float target;
	float chip;
	float bot;
	float top;
	float env;
	float rem;
	float volt;
	float curr;
	bool h_one;
	bool h_two;
};

bool start_heater_task();