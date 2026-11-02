package bridgescenario;

import basemod.BaseMod;
import basemod.interfaces.PostUpdateSubscriber;
import com.badlogic.gdx.Gdx;
import com.badlogic.gdx.Graphics;
import com.badlogic.gdx.math.RandomXS128;
import com.evacipated.cardcrawl.modthespire.lib.SpireInitializer;
import com.megacrit.cardcrawl.cards.AbstractCard;
import com.megacrit.cardcrawl.core.CardCrawlGame;
import com.megacrit.cardcrawl.dungeons.AbstractDungeon;
import com.megacrit.cardcrawl.helpers.CardLibrary;
import com.megacrit.cardcrawl.helpers.PotionHelper;
import com.megacrit.cardcrawl.helpers.RelicLibrary;
import com.megacrit.cardcrawl.potions.AbstractPotion;
import com.megacrit.cardcrawl.relics.AbstractRelic;
import java.io.IOException;
import java.lang.reflect.Proxy;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Test-only: gives the player specific cards, relics and potions so mirror runs can reach things the agent rarely
 * sees. The bridge writes bridge_scenario_in.txt in the game folder, one tab-separated command per line, and this
 * applies it on the game thread and answers in bridge_scenario_out.txt. It does nothing unless that file appears.
 *
 * <p>The command cardRandomRng answers with that generator's state. The game's discovery action rolls it once per frame
 * while its screen opens, which no simulator can predict, so mirror mode copies the state afterwards.
 *
 * <p>Nearly all of a mirror run's time is spent waiting for animations, so STS_TIME_SCALE also speeds up the clock the
 * game reads. Game logic only uses that clock to time its animations and actions.
 */
@SpireInitializer
public class BridgeScenario implements PostUpdateSubscriber {
    private static final Path IN = Paths.get("bridge_scenario_in.txt");
    private static final Path OUT = Paths.get("bridge_scenario_out.txt");
    private static final Path OUT_TMP = Paths.get("bridge_scenario_out.tmp");

    private static final float SECRET_PORTAL_PLAYTIME = 800f;

    private boolean timeScaled = false;
    private boolean mirroring = false;

    public static void initialize() {
        BaseMod.subscribe(new BridgeScenario());
    }

    private static void scaleTime(String text) {
        if (text == null || text.isEmpty()) {
            return;
        }
        final float scale = Float.parseFloat(text);
        final Graphics real = Gdx.graphics;
        Gdx.graphics = (Graphics) Proxy.newProxyInstance(Graphics.class.getClassLoader(), new Class<?>[] {Graphics.class},
                (proxy, method, args) -> {
                    Object result = method.invoke(real, args);
                    String name = method.getName();
                    boolean isClock = name.equals("getDeltaTime") || name.equals("getRawDeltaTime");
                    return isClock ? Float.valueOf((Float) result * scale) : result;
                });
    }

    @Override
    public void receivePostUpdate() {
        // Not in initialize: the game's window sets Gdx.graphics after mods have initialized.
        if (!timeScaled) {
            timeScaled = true;
            String scale = System.getenv("STS_TIME_SCALE");
            mirroring = scale != null && !scale.isEmpty();
            scaleTime(scale);
        }
        // Secret Portal joins the act 3 events after 800 seconds of play, and the simulator assumes it has. A mirror run's
        // real time shouldn't decide which event comes up.
        if (mirroring && CardCrawlGame.playtime < SECRET_PORTAL_PLAYTIME) {
            CardCrawlGame.playtime = SECRET_PORTAL_PLAYTIME;
        }
        if (!Files.exists(IN) || AbstractDungeon.player == null) {
            return;
        }
        String answer = "ok";
        try {
            for (String line : Files.readAllLines(IN, StandardCharsets.UTF_8)) {
                if (line.equals("cardRandomRng")) {
                    RandomXS128 random = AbstractDungeon.cardRandomRng.random;
                    answer += "\t" + random.getState(0) + "\t" + random.getState(1) + "\t" + AbstractDungeon.cardRandomRng.counter;
                } else if (!line.isEmpty()) {
                    apply(line.split("\t"));
                }
            }
            Files.delete(IN);
        } catch (Exception e) {
            answer = "error: " + e;
            IN.toFile().delete();
        }
        try {
            Files.write(OUT_TMP, Arrays.asList(answer), StandardCharsets.UTF_8);
            Files.move(OUT_TMP, OUT, StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    private static void apply(String[] command) {
        switch (command[0]) {
            case "relic":
                giveRelic(command[1]);
                break;
            case "card":
                giveCard(command[1], Integer.parseInt(command[2]));
                break;
            case "potion":
                givePotion(command[1]);
                break;
            case "front":
                putAtFrontOfPool(command[1], command[2]);
                break;
            case "monster":
                AbstractDungeon.monsterList.set(0, command[1]);
                break;
            case "elite":
                AbstractDungeon.eliteMonsterList.set(0, command[1]);
                break;
            default:
                throw new IllegalArgumentException("unknown command " + command[0]);
        }
    }

    private static void giveRelic(String id) {
        if (!RelicLibrary.isARelic(id)) {
            throw new IllegalArgumentException("unknown relic " + id);
        }
        removeFromPools(id);
        RelicLibrary.getRelic(id).makeCopy().instantObtain();
    }

    private static void giveCard(String id, int upgrades) {
        AbstractCard card = CardLibrary.getCard(id);
        if (card == null) {
            throw new IllegalArgumentException("unknown card " + id);
        }
        AbstractCard copy = card.makeCopy();
        for (int i = 0; i < upgrades; ++i) {
            copy.upgrade();
        }
        AbstractDungeon.player.masterDeck.addToTop(copy);
    }

    private static void givePotion(String id) {
        AbstractPotion potion = PotionHelper.getPotion(id);
        if (potion == null) {
            throw new IllegalArgumentException("unknown potion " + id);
        }
        AbstractDungeon.player.obtainPotion(potion);
    }

    private static void putAtFrontOfPool(String tier, String id) {
        if (!RelicLibrary.isARelic(id)) {
            throw new IllegalArgumentException("unknown relic " + id);
        }
        removeFromPools(id);
        pool(tier).add(0, id);
    }

    private static ArrayList<String> pool(String tier) {
        switch (tier) {
            case "common":
                return AbstractDungeon.commonRelicPool;
            case "uncommon":
                return AbstractDungeon.uncommonRelicPool;
            case "rare":
                return AbstractDungeon.rareRelicPool;
            case "shop":
                return AbstractDungeon.shopRelicPool;
            case "boss":
                return AbstractDungeon.bossRelicPool;
            default:
                throw new IllegalArgumentException("unknown relic tier " + tier);
        }
    }

    private static void removeFromPools(String id) {
        List<ArrayList<String>> pools = Arrays.asList(AbstractDungeon.commonRelicPool, AbstractDungeon.uncommonRelicPool,
                AbstractDungeon.rareRelicPool, AbstractDungeon.shopRelicPool, AbstractDungeon.bossRelicPool);
        for (ArrayList<String> pool : pools) {
            pool.remove(id);
        }
    }
}
