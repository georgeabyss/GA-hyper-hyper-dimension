// Entry point of the null test. The work happens in NullTest.cpp, which is compiled into the
// plug-in's shared code so it runs the processor exactly as the plug-in builds it.
int hhdNullTest (int argc, char** argv);

int main (int argc, char** argv)
{
    return hhdNullTest (argc, argv);
}
