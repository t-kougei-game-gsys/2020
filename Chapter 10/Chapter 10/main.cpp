#include "Application.h"

int main () {
	auto& app = Application::Instance ();
	if (!app.Init ()) {
		printf_s ("Application Init is failed.\n");
		return -1;
	}

	app.Run ();
	app.Terminate ();

	return 0;
}