/**
 * Configuration for the Cibyl application. If you want to edit these
 * settings, copy it to your application directory, edit it and
 * install it into src/ when compiling your application.
 */
class CibylConfig
{
    /** Argument to GameCanvas constructor */
    public static final boolean supressKeyEvents = false;

    /** Memory size - 0 means use default size (Runtime.freeMemory() * CibylConfig.cibylMemoryProportion) */
    public static int memorySize = 4500000;
    //public static int memorySize = 2000000;

    /** The proportion of memory allocated to Cibyl (0..1) */
    public static float cibylMemoryProportion = 0.7f;

    /** Stack size - 8KB by default */
    public static int stackSize = 8192;

    /** Event stack size - 4KB by default */
    public static int eventStackSize = 4096;

    /** The encoding of strings */
    public static String stringEncoding = "UTF8";

    /** Disable the addition of the Exit command */
    public static boolean disableExitCmd = true;
}
