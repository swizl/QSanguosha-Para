#include "special3v3.h"
#include "skill.h"
#include "standard.h"
#include "server.h"
#include "engine.h"
#include "ai.h"
#include "maneuvering.h"
#include "clientplayer.h"

HongyuanCard::HongyuanCard() {
    mute = true;
}

bool HongyuanCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const{
    return to_select != Self && targets.length() < 2;
}

void HongyuanCard::onEffect(const CardEffectStruct &effect) const{
   effect.to->drawCards(1);
}

class HongyuanViewAsSkill: public ZeroCardViewAsSkill {
public:
    HongyuanViewAsSkill():ZeroCardViewAsSkill("hongyuan") {
    }

    virtual const Card *viewAs() const{
        return new HongyuanCard;
    }

    virtual bool isEnabledAtPlay(const Player *) const{
        return false;
    }

    virtual bool isEnabledAtResponse(const Player *, const QString &pattern) const{
        return pattern == "@@hongyuan";
    }
};

class Hongyuan: public DrawCardsSkill {
public:
    Hongyuan(): DrawCardsSkill("hongyuan") {
        frequency = NotFrequent;
        view_as_skill = new HongyuanViewAsSkill;
    }

    QStringList getTeammateNames(ServerPlayer *zhugejin) const{
        Room *room = zhugejin->getRoom();

        QStringList names;
        foreach (ServerPlayer *other, room->getOtherPlayers(zhugejin)) {
            if (AI::GetRelation3v3(zhugejin, other) == AI::Friend)
                names << other->getGeneralName();
        }
        return names;
    }

    virtual int getDrawNum(ServerPlayer *zhugejin, int n) const{
        Room *room = zhugejin->getRoom();             
        if (room->askForSkillInvoke(zhugejin, objectName())) {
            room->broadcastSkillInvoke(objectName());
            zhugejin->setFlags("hongyuan");
            if (ServerInfo.GameMode == "06_3v3") {
                QStringList names = getTeammateNames(zhugejin);
                room->setTag("HongyuanTargets", QVariant::fromValue(names));
            }
            return n - 1;
        } else
            return n;
    }
};

class HongyuanDraw: public TriggerSkill {
public:
    HongyuanDraw(): TriggerSkill("#hongyuan") {
        events << AfterDrawNCards;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &) const{
        if (!player->hasFlag("hongyuan"))
            return false;
        player->setFlags("-hongyuan");

        QStringList names = room->getTag("HongyuanTargets").toStringList();
        room->removeTag("HongyuanTargets");
        if (!names.isEmpty())
            foreach (QString name, names)
                room->findPlayer(name)->drawCards(1);
        else
            room->askForUseCard(player, "@@hongyuan", "@hongyuan");
        return false;
    }
};

HuanshiCard::HuanshiCard() {
    target_fixed = true;
    will_throw = false;
    handling_method = Card::MethodResponse;
}

void HuanshiCard::use(Room *, ServerPlayer *, QList<ServerPlayer *> &) const{
}

class HuanshiViewAsSkill: public OneCardViewAsSkill {
public:
    HuanshiViewAsSkill(): OneCardViewAsSkill("huanshi") {
    }

    virtual bool isEnabledAtPlay(const Player *) const{
        return false;
    }

    virtual bool isEnabledAtResponse(const Player *, const QString &pattern) const{
        return pattern == "@huanshi";
    }

    virtual bool viewFilter(const Card *card) const{
        return !Self->isCardLimited(card, Card::MethodResponse);
    }

    virtual const Card *viewAs(const Card *to_select) const{
        Card *card = new HuanshiCard;
        card->setSuit(to_select->getSuit());
        card->addSubcard(to_select);

        return card;
    }
};

class Huanshi: public TriggerSkill {
public:
    Huanshi(): TriggerSkill("huanshi") {
        events << AskForRetrial;
        view_as_skill = new HuanshiViewAsSkill;
    }

    QList<ServerPlayer *> getTeammates(ServerPlayer *zhugejin) const{
        Room *room = zhugejin->getRoom();

        QList<ServerPlayer *> teammates;
        teammates << zhugejin;
        foreach (ServerPlayer *other, room->getOtherPlayers(zhugejin)) {
            if (AI::GetRelation3v3(zhugejin, other) == AI::Friend)
                teammates << other;
        }
        return teammates;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return TriggerSkill::triggerable(target) && !target->isNude();
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const{
        JudgeStar judge = data.value<JudgeStar>();

        bool can_invoke = false;
        if (ServerInfo.GameMode == "06_3v3") {
            foreach (ServerPlayer *teammate, getTeammates(player)) {
                if (teammate == judge->who) {
                    can_invoke = true;
                    break;
                }
            }
         } else if (!player->isNude())
            can_invoke = (judge->who == player || room->askForChoice(judge->who, "huanshi", "accept+reject") == "accept");

        if (!can_invoke)
            return false;

        QStringList prompt_list;
        prompt_list << "@huanshi-card" << judge->who->objectName()
                    << objectName() << judge->reason << QString::number(judge->card->getEffectiveId());
        QString prompt = prompt_list.join(":");

        player->tag["Judge"] = data;
        const Card *card = room->askForCard(player, "@huanshi", prompt, data, Card::MethodResponse, judge->who, true);
        if (card != NULL) {
            room->broadcastSkillInvoke(objectName());
            room->retrial(card, player, judge, objectName());
        }

        return false;
    }
};

class Mingzhe: public TriggerSkill {
public:
    Mingzhe(): TriggerSkill("mingzhe") {
        events << CardsMoveOneTime;
        frequency = Frequent;
    }

    virtual bool trigger(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data) const{
        if (player->getPhase() != Player::NotActive)
            return false;

        CardsMoveOneTimeStar move = data.value<CardsMoveOneTimeStar>();
        if (move->from != player)
            return false;

        CardMoveReason reason = move->reason;

        if ((reason.m_reason & CardMoveReason::S_MASK_BASIC_REASON) == CardMoveReason::S_REASON_USE
            || (reason.m_reason & CardMoveReason::S_MASK_BASIC_REASON) == CardMoveReason::S_REASON_DISCARD
            || (reason.m_reason & CardMoveReason::S_MASK_BASIC_REASON) == CardMoveReason::S_REASON_RESPONSE) {
            const Card *card;
            int i = 0;
            foreach (int card_id, move->card_ids) {
                card = Sanguosha->getCard(card_id);
                if (card->isRed() && (move->from_places[i] == Player::PlaceHand
                                      || move->from_places[i] == Player::PlaceEquip)
                    && player->askForSkillInvoke(objectName(), data)) {
                    room->broadcastSkillInvoke(objectName());
                    player->drawCards(1);
                }
                i++;
            }
        }
        return false;
    }
};

New3v3CardPackage::New3v3CardPackage()
    : Package("New3v3Card")
{
    QList<Card *> cards;
    cards << new SupplyShortage(Card::Spade, 1)
          << new SupplyShortage(Card::Club, 12)
          << new Nullification(Card::Heart, 12);

    foreach (Card *card, cards)
        card->setParent(this);

    type = CardPack;
}

ADD_PACKAGE(New3v3Card)

Special3v3Package::Special3v3Package():Package("Special3v3")
{
    General *zhugejin = new General(this, "zhugejin", "wu", 3, true);
    zhugejin->addSkill(new Hongyuan);
    zhugejin->addSkill(new HongyuanDraw);
    zhugejin->addSkill(new Huanshi);
    zhugejin->addSkill(new Mingzhe);
    related_skills.insertMulti("hongyuan", "#hongyuan");

    addMetaObject<HuanshiCard>();
    addMetaObject<HongyuanCard>();
}

ADD_PACKAGE(Special3v3)

