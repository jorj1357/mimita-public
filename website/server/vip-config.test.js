import test from "node:test"
import assert from "node:assert/strict"
import {
    lifetimeBadgeForTier,
    prepaidAmountCents,
    prepaidPurchaseDefinition,
    publicVipConfig
} from "./vip-config.js"

test("prepaid slider price grows with a linear discount and ends at half price", () => {
    assert.equal(prepaidAmountCents("vip", 1), 333)
    assert.equal(prepaidAmountCents("vip", 12), 1998)
    assert.equal(prepaidAmountCents("super_vip", 12), 5328)
    assert.equal(prepaidAmountCents("ultra_vip", 12), 10662)
    assert.equal(prepaidPurchaseDefinition("vip", 8).calendar_months, 8)
    assert.equal(prepaidAmountCents("vip", 0), 0)
    assert.equal(prepaidAmountCents("vip", 13), 0)
})

test("lifetime badges and configured prices are exposed", () => {
    assert.match(lifetimeBadgeForTier("vip"), /mimita-vip-lifetime-v1\.png$/)
    const config = publicVipConfig({
        STRIPE_SECRET_KEY: "sk_test_example",
        MIMITA_STRIPE_PRICE_VIP_MONTHLY: "price_vip_monthly",
        MIMITA_STRIPE_PRICE_VIP_LIFETIME: "price_vip_lifetime"
    })
    const vip = config.tiers.find(tier => tier.tier === "vip")
    assert.equal(vip.purchases.find(option => option.type === "prepaid").configured, true)
    assert.equal(vip.purchases.find(option => option.type === "lifetime").configured, true)
})
