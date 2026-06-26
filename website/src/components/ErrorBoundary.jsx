import { Component } from "react"

export default class ErrorBoundary extends Component {
    constructor(props) {
        super(props)
        this.state = { error: null, details: false }
    }

    static getDerivedStateFromError(error) {
        return { error }
    }

    componentDidCatch(error, info) {
        console.log("[ERROR BOUNDARY] Caught:", error.message)
        console.log("[ERROR BOUNDARY] Stack:", error.stack)
        console.log("[ERROR BOUNDARY] Component stack:", info?.componentStack || "none")
    }

    render() {
        if (this.state.error) {
            return (
                <div className="adminPage">
                    <div className="errorBoundaryCard">
                        <h2>something went wrong</h2>
                        <p className="adminError">{this.state.error.message}</p>
                        <div className="errorBoundaryActions">
                            <button className="adminRefreshBtn" onClick={() => this.setState({ error: null, details: false })}>
                                retry
                            </button>
                            <button className="adminRefreshBtn" onClick={() => window.location.reload()}>
                                reload page
                            </button>
                            <button
                                className="adminDebugBtn"
                                onClick={() => this.setState(s => ({ details: !s.details }))}
                            >
                                {this.state.details ? "hide details" : "show details"}
                            </button>
                        </div>
                        {this.state.details && (
                            <pre className="errorBoundaryStack">{this.state.error.stack}</pre>
                        )}
                    </div>
                </div>
            )
        }
        return this.props.children
    }
}
