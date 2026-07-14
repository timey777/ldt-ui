#include <app_delegate.h>

int main(int argc, char* argv[])
{
	AppDelegate app("gui", 800, 600);
	return app.run(argc, argv);
}
