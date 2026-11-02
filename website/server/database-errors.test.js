import test from "node:test"
import assert from "node:assert/strict"
import { isDatabaseUnavailable } from "./database-errors.js"

test("recognizes PostgreSQL connection refusal wrapped in AggregateError", () => {
    const error = new AggregateError([Object.assign(new Error("refused"), {
        code: "ECONNREFUSED"
    })])
    assert.equal(isDatabaseUnavailable(error), true)
})

test("recognizes PostgreSQL shutdown and timeout states", () => {
    assert.equal(isDatabaseUnavailable({ code: "57P03" }), true)
    assert.equal(isDatabaseUnavailable({ code: "ETIMEDOUT" }), true)
})

test("does not classify query or constraint errors as database outages", () => {
    assert.equal(isDatabaseUnavailable({ code: "23505" }), false)
    assert.equal(isDatabaseUnavailable({ code: "42P01" }), false)
    assert.equal(isDatabaseUnavailable(new Error("unknown")), false)
})
