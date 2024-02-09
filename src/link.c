// link shim

int
link_serial_new(void)
{
	static int serial = 0;

	return serial++;
}
