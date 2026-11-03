package bridgescenario;

import com.megacrit.cardcrawl.dungeons.AbstractDungeon;
import com.megacrit.cardcrawl.random.Random;
import java.io.IOException;
import java.lang.reflect.Field;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.file.StandardOpenOption;

/**
 * Debug only: STS_TRACE_RNG=cardRng,miscRng logs who rolls those streams to bridge_rng_trace.txt in the game folder.
 * It shares its generator with the stream it replaces, so the rolls themselves don't change.
 */
class TracingRandom extends Random {
    private static final int STACK_DEPTH = 8;

    private final String name;

    private TracingRandom(String name, Random stream) {
        this.name = name;
        this.random = stream.random;
        this.counter = stream.counter;
    }

    /** Wraps the named streams. The game replaces them when a run starts, so call this every frame. */
    static void install(String names) {
        if (names == null || names.isEmpty()) {
            return;
        }
        for (String name : names.split(",")) {
            try {
                Field field = AbstractDungeon.class.getField(name);
                Random stream = (Random) field.get(null);
                if (stream != null && !(stream instanceof TracingRandom)) {
                    field.set(null, new TracingRandom(name, stream));
                }
            } catch (ReflectiveOperationException e) {
                throw new IllegalArgumentException("no stream " + name, e);
            }
        }
    }

    private void log() {
        // The base constructor rolls to advance its own generator, which is replaced straight after.
        if (name == null) {
            return;
        }
        StringBuilder line = new StringBuilder(name).append(" #").append(counter);
        StackTraceElement[] stack = new Throwable().getStackTrace();
        for (int i = 2; i < stack.length && i < 2 + STACK_DEPTH; ++i) {
            line.append(" < ").append(stack[i].getClassName()).append('.').append(stack[i].getMethodName());
        }
        line.append('\n');
        try {
            Files.write(Paths.get("bridge_rng_trace.txt"), line.toString().getBytes(StandardCharsets.UTF_8),
                StandardOpenOption.CREATE, StandardOpenOption.APPEND);
        } catch (IOException e) {
            throw new IllegalStateException("can't write the trace", e);
        }
    }

    @Override
    public int random(int range) {
        log();
        return super.random(range);
    }

    @Override
    public int random(int start, int end) {
        log();
        return super.random(start, end);
    }

    @Override
    public long random(long range) {
        log();
        return super.random(range);
    }

    @Override
    public long random(long start, long end) {
        log();
        return super.random(start, end);
    }

    @Override
    public long randomLong() {
        log();
        return super.randomLong();
    }

    @Override
    public boolean randomBoolean() {
        log();
        return super.randomBoolean();
    }

    @Override
    public boolean randomBoolean(float chance) {
        log();
        return super.randomBoolean(chance);
    }

    @Override
    public float random() {
        log();
        return super.random();
    }

    @Override
    public float random(float range) {
        log();
        return super.random(range);
    }

    @Override
    public float random(float start, float end) {
        log();
        return super.random(start, end);
    }
}
