set -e
make FINALPACKAGE=1

if [ -d ".theos/Payload" ]
then
    rm -rf .theos/Payload
fi

if [ -f "packages/TimeBomb2.tipa" ]
then
    rm -rf packages/TimeBomb2.tipa
fi

mkdir .theos/Payload
mv .theos/obj/TimeBomb2.app .theos/Payload/TimeBomb2.app
cp .theos/obj/spinlock_child .theos/Payload/TimeBomb2.app/spinlock_child

mkdir -p packages
cd .theos
zip -vr ../packages/TimeBomb2.tipa Payload
cd -

rm -rf .theos/Payload