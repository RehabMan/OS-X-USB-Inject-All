#!/bin/bash

plist=./USBInjectAll/USBInjectAll-Info.plist
plist_temp=/tmp/org_rehabman_USBInjectAll-Info.plist
merge=./USBInjectAll_model_template.plist

function mergeModelData
{
    buddy="/usr/libexec/plistbuddy -c"
    tmp=/tmp/USB_translated.plist
    cat $merge | perl -p -e "s/model_placeholder/$1/" >$tmp
    $buddy "Merge $tmp ':IOKitPersonalities'" $plist_temp
}

# start with blank template (no model injectors)
cp ./USBInjectAll_template-Info.plist $plist_temp

# MacBookPro
mergeModelData "MacBookPro8,1"
mergeModelData "MacBookPro8,2"
mergeModelData "MacBookPro8,3"
mergeModelData "MacBookPro9,1"
mergeModelData "MacBookPro9,2"
mergeModelData "MacBookPro10,1"
mergeModelData "MacBookpro10,2"
mergeModelData "MacBookPro11,1"
mergeModelData "MacBookPro11,2"
mergeModelData "MacBookPro12,1"
mergeModelData "MacBookPro12,2"

# MacBookAir
mergeModelData "MacBookAir4,1"
mergeModelData "MacBookAir4,2"
mergeModelData "MacBookAir5,1"
mergeModelData "MacBookAir5,2"
mergeModelData "MacBookAir6,1"
mergeModelData "MacBookAir6,2"
mergeModelData "MacBookAir7,1"
mergeModelData "MacBookAir7,2"

# iMac
mergeModelData "iMac12,1"
mergeModelData "iMac12,2"
mergeModelData "iMac13,1"
mergeModelData "iMac13,2"
mergeModelData "iMac14,1"
mergeModelData "iMac14,2"
mergeModelData "iMac14,3"
mergeModelData "iMac15,1"
mergeModelData "iMac16,1"
mergeModelData "iMac16,2"

# Macmini
mergeModelData "Macmini5,1"
mergeModelData "Macmini5,2"
mergeModelData "Macmini5,3"
mergeModelData "Macmini6,1"
mergeModelData "Macmini6,2"
mergeModelData "Macmini7,1"

# MacBook
mergeModelData "MacBook8,1"

cksum_old=`md5 -q $plist`
cksum_new=`md5 -q $plist_temp`
if [[ $cksum_new != $cksum_old ]]; then
    cp $plist_temp $plist
    echo "Updated Info.plist"
else
    echo "Generated Info.plist is same as old, no need to update"
fi
