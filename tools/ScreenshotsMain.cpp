// Entry point of the screenshot tool. The work happens in Screenshots.cpp, which is compiled
// into the plug-in's shared code so it sees exactly the same JUCE configuration as the plug-in.
int hhdRenderScreenshots (const char* outputFolder);

int main (int argc, char** argv)
{
    return hhdRenderScreenshots (argc > 1 ? argv[1] : "docs");
}
